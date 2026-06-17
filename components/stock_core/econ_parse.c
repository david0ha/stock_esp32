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

/* True if the Mon..Sun week `monday0 + 7*w` (in day numbers) intersects the
 * month spanning day numbers [first, last]. */
static bool week_overlaps_month(int64_t monday0, int64_t first, int64_t last, int w) {
    int64_t mon = monday0 + (int64_t)w * 7;
    return mon <= last && mon + 6 >= first;
}

void econ_month_week_span(time_t now_utc, long tz_off, int *w_min, int *w_max) {
    time_t local = now_utc + tz_off;
    struct tm t;
    gmtime_r(&local, &t);

    int Y = t.tm_year + 1900, M = t.tm_mon + 1;
    int64_t first = days_from_civil(Y, M, 1);
    int ny = (M == 12) ? Y + 1 : Y, nm = (M == 12) ? 1 : M + 1;
    int64_t last  = days_from_civil(ny, nm, 1) - 1;          /* last day of month */

    /* Monday of the current week as a day number. Derive it by stepping back in
     * seconds then reading the civil date (like econ_week_range), so it floors
     * correctly even if `local` is negative (clock unset before SNTP). */
    int wday_from_mon = (t.tm_wday + 6) % 7;                 /* Mon=0 .. Sun=6     */
    time_t monday = local - (time_t)wday_from_mon * SECS_PER_DAY;
    struct tm tm_mon;
    gmtime_r(&monday, &tm_mon);
    int64_t monday0 = days_from_civil(tm_mon.tm_year + 1900, tm_mon.tm_mon + 1, tm_mon.tm_mday);

    int lo = 0, hi = 0;                                      /* current week always overlaps */
    while (week_overlaps_month(monday0, first, last, lo - 1)) lo--;
    while (week_overlaps_month(monday0, first, last, hi + 1)) hi++;
    if (w_min) *w_min = lo;
    if (w_max) *w_max = hi;
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
