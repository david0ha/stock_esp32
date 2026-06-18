/*
 * econ_parse.c — FMP economics-calendar JSON + time helpers (cJSON).
 *
 * Pure functions: no network, no globals. Unit-tested on the host against
 * captured FMP fixtures (components/stock_core/test/host). The firmware and the
 * desktop simulator compile this exact file.
 */
#include "econ_parse.h"
#include "stock_text.h"   /* copy_cstr / to_ascii (shared with stock_parse.c) */
#include "cJSON.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>   /* strcasecmp */
#include <ctype.h>     /* toupper */

#define SECS_PER_DAY 86400

/* ---- time helpers ------------------------------------------------------- */

/* UTC civil date -> days since 1970-01-01 (Howard Hinnant's algorithm). Valid
 * for any Gregorian date; no libc/timezone dependency so it matches everywhere. */
static int64_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int yoe = (int)(y - era * 400);                              /* [0, 399]      */
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;    /* [0, 365]      */
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;             /* [0, 146096]   */
    return era * 146097 + doe - 719468;
}

int64_t econ_ymd_to_epoch(int year, int month, int day, int hour, int min, int sec) {
    return days_from_civil(year, month, day) * SECS_PER_DAY
         + (int64_t)hour * 3600 + (int64_t)min * 60 + sec;
}

/* Epoch (UTC) + tz offset -> short "Ddd HH:MM" (weekday + time) in the
 * device-local zone. The week label already carries the date range, so a per-row
 * weekday is enough context and keeps the column narrow. We add the offset and
 * read it back with gmtime_r so the result is independent of the host's own TZ. */
static void fmt_local_when(char *buf, size_t n, int64_t epoch_utc, long tz_off) {
    time_t t = (time_t)(epoch_utc + tz_off);
    struct tm tm;
    if (gmtime_r(&t, &tm)) strftime(buf, n, "%a %H:%M", &tm);
    else if (n)           buf[0] = '\0';
}

int econ_impact_from_str(const char *s) {
    if (!s) return ECON_IMPACT_NONE;
    if (strcasecmp(s, "High")   == 0) return ECON_IMPACT_HIGH;
    if (strcasecmp(s, "Medium") == 0) return ECON_IMPACT_MEDIUM;
    if (strcasecmp(s, "Low")    == 0) return ECON_IMPACT_LOW;
    return ECON_IMPACT_NONE;
}

long econ_local_tz_off(time_t now) {
    struct tm lt, gt;
    localtime_r(&now, &lt);
    gmtime_r(&now, &gt);
    long off = (lt.tm_hour - gt.tm_hour) * 3600
             + (lt.tm_min  - gt.tm_min)  * 60
             + (lt.tm_sec  - gt.tm_sec);
    int dday = (lt.tm_year != gt.tm_year) ? (lt.tm_year > gt.tm_year ? 1 : -1)
                                          : (lt.tm_yday - gt.tm_yday);
    return off + dday * 86400;
}

void econ_week_range(time_t now_utc, long tz_off, int week_offset,
                     char *from, size_t from_sz, char *to, size_t to_sz,
                     char *label, size_t label_sz) {
    /* Work in "local epoch" (UTC shifted by tz_off) and read it back with
     * gmtime_r, so weekday/date are the device-local ones. */
    time_t local = now_utc + tz_off + (time_t)week_offset * 7 * SECS_PER_DAY;
    struct tm t;
    gmtime_r(&local, &t);

    int wday_from_mon = (t.tm_wday + 6) % 7;            /* Mon=0 .. Sun=6 */
    time_t monday = local - (time_t)wday_from_mon * SECS_PER_DAY;
    time_t sunday = monday + 6 * SECS_PER_DAY;

    struct tm tm_mon, tm_sun;
    gmtime_r(&monday, &tm_mon);
    gmtime_r(&sunday, &tm_sun);
    if (from_sz) strftime(from, from_sz, "%Y-%m-%d", &tm_mon);
    if (to_sz)   strftime(to,   to_sz,   "%Y-%m-%d", &tm_sun);

    if (label_sz) {
        char a[8], b[8];
        strftime(a, sizeof(a), "%m-%d", &tm_mon);
        strftime(b, sizeof(b), "%m-%d", &tm_sun);
        snprintf(label, label_sz, "%s ~ %s", a, b);
    }
}

/* ---- parsing ------------------------------------------------------------ */

/* An estimate/actual/previous field -> short display text. FMP sends numbers;
 * the investing.com proxy sends unit-bearing strings ("1.00%", "262K"). Show a
 * number with trimmed decimals, a non-empty string verbatim (ASCII-folded), and
 * anything absent/null/empty as "--". */
static void fmt_field(char *dst, size_t n, const cJSON *obj, const char *key) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(it) && isfinite(it->valuedouble)) {
        double v = it->valuedouble, a = v < 0 ? -v : v;
        snprintf(dst, n, a >= 1000.0 ? "%.0f" : "%.2f", v);
    } else if (cJSON_IsString(it) && it->valuestring[0]) {
        to_ascii(dst, n, it->valuestring);
    } else {
        snprintf(dst, n, "--");
    }
}

/* Insert `e` into out->items keeping it sorted ascending by ts and capped at
 * ECON_EVENT_MAX. When full, an event later than every kept one is dropped so we
 * always retain the *earliest* ECON_EVENT_MAX of the week. */
static void insert_sorted(econ_calendar_t *out, const econ_event_t *e) {
    int n = out->count;
    int pos = n;
    while (pos > 0 && out->items[pos - 1].ts > e->ts) pos--;

    if (n < ECON_EVENT_MAX) {
        for (int i = n; i > pos; i--) out->items[i] = out->items[i - 1];
        out->items[pos] = *e;
        out->count = n + 1;
    } else if (pos < ECON_EVENT_MAX) {           /* earlier than the current last */
        for (int i = ECON_EVENT_MAX - 1; i > pos; i--) out->items[i] = out->items[i - 1];
        out->items[pos] = *e;
    }   /* else: full and this event is the latest -> drop (still counted) */
}

int econ_parse_calendar(const char *json, long tz_off, int min_impact,
                        econ_calendar_t *out) {
    memset(out, 0, sizeof(*out));

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        copy_cstr(out->error, sizeof(out->error), "bad response (not JSON)");
        return -1;
    }

    /* FMP signals auth/limit problems with an object carrying "Error Message"
     * instead of the expected array. Surface it verbatim. */
    if (!cJSON_IsArray(root)) {
        const cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "Error Message");
        copy_cstr(out->error, sizeof(out->error),
                  cJSON_IsString(msg) ? msg->valuestring : "unexpected response");
        cJSON_Delete(root);
        return -1;
    }

    const cJSON *ev;
    cJSON_ArrayForEach(ev, root) {
        if (!cJSON_IsObject(ev)) continue;

        const cJSON *impact = cJSON_GetObjectItemCaseSensitive(ev, "impact");
        int rank = econ_impact_from_str(cJSON_IsString(impact) ? impact->valuestring : NULL);
        if (rank < min_impact) continue;

        const cJSON *date = cJSON_GetObjectItemCaseSensitive(ev, "date");
        if (!cJSON_IsString(date)) continue;
        int Y, Mo, D, h = 0, m = 0, s = 0;
        if (sscanf(date->valuestring, "%d-%d-%d %d:%d:%d", &Y, &Mo, &D, &h, &m, &s) < 3)
            continue;                                    /* unparseable timestamp */

        econ_event_t e;
        memset(&e, 0, sizeof(e));
        e.ts     = econ_ymd_to_epoch(Y, Mo, D, h, m, s);
        e.impact = rank;
        fmt_local_when(e.when, sizeof(e.when), e.ts, tz_off);

        /* country/event are API free-text -> fold to ASCII (mono font has no
         * accented letters / dashes) like the news parser does. */
        const cJSON *country = cJSON_GetObjectItemCaseSensitive(ev, "country");
        const cJSON *name    = cJSON_GetObjectItemCaseSensitive(ev, "event");
        to_ascii(e.country, sizeof(e.country), cJSON_IsString(country) ? country->valuestring : "");
        to_ascii(e.event,   sizeof(e.event),   cJSON_IsString(name)    ? name->valuestring    : "");
        fmt_field(e.estimate, sizeof(e.estimate), ev, "estimate");
        fmt_field(e.actual,   sizeof(e.actual),   ev, "actual");
        fmt_field(e.previous, sizeof(e.previous), ev, "previous");

        out->total_matched++;
        insert_sorted(out, &e);
    }

    cJSON_Delete(root);
    out->valid = true;
    return 0;
}

int econ_next_after(const econ_calendar_t *cal, int64_t now_utc) {
    if (!cal || !cal->valid) return -1;
    for (int i = 0; i < cal->count; i++)
        if (cal->items[i].ts > now_utc) return i;
    return -1;
}

void econ_when_label(int64_t ts, time_t now, long tz_off, char *out, size_t n) {
    if (!out || n == 0) return;
    time_t ev_local  = (time_t)(ts  + tz_off);
    time_t now_local = (time_t)(now + tz_off);
    struct tm evt, nwt;
    gmtime_r(&ev_local,  &evt);
    gmtime_r(&now_local, &nwt);

    int64_t ev_day  = days_from_civil(evt.tm_year + 1900, evt.tm_mon + 1, evt.tm_mday);
    int64_t now_day = days_from_civil(nwt.tm_year + 1900, nwt.tm_mon + 1, nwt.tm_mday);
    int64_t d = ev_day - now_day;

    char hm[8];
    strftime(hm, sizeof hm, "%H:%M", &evt);

    if (d == 0) {
        snprintf(out, n, "TODAY %s", hm);
    } else if (d == 1) {
        snprintf(out, n, "TOMORROW %s", hm);
    } else if (d >= 2 && d <= 6) {
        char wd[8];
        strftime(wd, sizeof wd, "%a", &evt);          /* "Mon".."Sun" */
        for (char *p = wd; *p; ++p) *p = (char)toupper((unsigned char)*p);
        snprintf(out, n, "%s %s", wd, hm);
    } else {
        char md[8];
        strftime(md, sizeof md, "%m-%d", &evt);
        snprintf(out, n, "%s %s", md, hm);
    }
}
