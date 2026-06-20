/*
 * weather_parse.c — see weather_parse.h. Parses the two Open-Meteo JSON shapes
 * (geocoding + forecast) into the portable weather model. Shared verbatim by the
 * firmware, the desktop simulator, and the host tests.
 */
#include "weather_parse.h"
#include "cJSON.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* WMO 4677 weather-interpretation codes -> the four panel glyphs.
 *   0          clear                -> SUN
 *   1,2        mainly clear / partly-> PARTLY
 *   3,45,48    overcast / fog       -> CLOUD
 *   51..99     drizzle/rain/snow/ts -> RAIN  (snow collapses onto rain)
 */
wx_kind_t wx_from_wmo(int code) {
    if (code <= 0)            return WX_SUN;
    if (code == 1 || code == 2) return WX_PARTLY;
    if (code == 3 || code == 45 || code == 48) return WX_CLOUD;
    if (code >= 51 && code <= 99) return WX_RAIN;
    return WX_CLOUD;
}

/* Day-of-week (0=Sunday) for a Gregorian date via Sakamoto's algorithm. */
static int day_of_week(int y, int m, int d) {
    static const int t[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

/* "YYYY-MM-DD..." -> uppercase 3-letter weekday into out[4]. Leaves out empty
 * if the date can't be read. */
static void date_to_dow(const char *iso, char *out) {
    static const char *DOW[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    out[0] = '\0';
    int y, m, d;
    if (!iso || sscanf(iso, "%d-%d-%d", &y, &m, &d) != 3) return;
    if (m < 1 || m > 12 || d < 1 || d > 31) return;
    int w = day_of_week(y, m, d);
    if (w < 0) w += 7;
    snprintf(out, 4, "%s", DOW[w % 7]);
}

static int round_c(double v) {
    return (int)lround(v);
}

int geo_parse(const char *json, geo_loc_t *out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!json) return -1;

    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");
    cJSON *first = (cJSON_IsArray(results) && cJSON_GetArraySize(results) > 0)
                       ? cJSON_GetArrayItem(results, 0) : NULL;
    if (!first) { cJSON_Delete(root); return -1; }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(first, "name");
    const cJSON *cc   = cJSON_GetObjectItemCaseSensitive(first, "country_code");
    const cJSON *lat  = cJSON_GetObjectItemCaseSensitive(first, "latitude");
    const cJSON *lon  = cJSON_GetObjectItemCaseSensitive(first, "longitude");

    if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lon)) { cJSON_Delete(root); return -1; }

    if (cJSON_IsString(name) && name->valuestring)
        snprintf(out->name, sizeof(out->name), "%s", name->valuestring);
    if (cJSON_IsString(cc) && cc->valuestring)
        snprintf(out->country, sizeof(out->country), "%s", cc->valuestring);
    out->lat   = lat->valuedouble;
    out->lon   = lon->valuedouble;
    out->valid = true;

    cJSON_Delete(root);
    return 0;
}

int weather_parse(const char *json, weather_t *out) {
    if (!out) return -1;
    memset(out, 0, sizeof(*out));
    if (!json) return -1;

    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    /* current conditions */
    cJSON *cur = cJSON_GetObjectItemCaseSensitive(root, "current");
    if (cur) {
        const cJSON *t  = cJSON_GetObjectItemCaseSensitive(cur, "temperature_2m");
        const cJSON *wc = cJSON_GetObjectItemCaseSensitive(cur, "weather_code");
        if (cJSON_IsNumber(t) && cJSON_IsNumber(wc)) {
            out->now_temp_c = round_c(t->valuedouble);
            out->now_wx     = wx_from_wmo((int)wc->valuedouble);
            out->now_valid  = true;
        }
    }

    /* daily forecast: parallel arrays time[] / weather_code[] / max[] / min[] */
    cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");
    if (daily) {
        const cJSON *time = cJSON_GetObjectItemCaseSensitive(daily, "time");
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(daily, "weather_code");
        const cJSON *hi   = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_max");
        const cJSON *lo   = cJSON_GetObjectItemCaseSensitive(daily, "temperature_2m_min");
        if (cJSON_IsArray(time) && cJSON_IsArray(code) &&
            cJSON_IsArray(hi)   && cJSON_IsArray(lo)) {
            int n = cJSON_GetArraySize(time);
            if (n > WX_FORECAST_MAX) n = WX_FORECAST_MAX;
            int k = 0;
            for (int i = 0; i < n; i++) {
                const cJSON *ti = cJSON_GetArrayItem(time, i);
                const cJSON *ci = cJSON_GetArrayItem(code, i);
                const cJSON *hii = cJSON_GetArrayItem(hi, i);
                const cJSON *loi = cJSON_GetArrayItem(lo, i);
                if (!cJSON_IsString(ti) || !cJSON_IsNumber(ci) ||
                    !cJSON_IsNumber(hii) || !cJSON_IsNumber(loi))
                    continue;
                wx_day_t *day = &out->days[k++];
                date_to_dow(ti->valuestring, day->dow);
                day->wx = wx_from_wmo((int)ci->valuedouble);
                day->hi = round_c(hii->valuedouble);
                day->lo = round_c(loi->valuedouble);
            }
            out->day_count = k;
        }
    }

    out->valid = out->now_valid || out->day_count > 0;
    cJSON_Delete(root);
    return out->valid ? 0 : -1;
}
