/*
 * Host unit tests for the economic-calendar parse/time helpers.
 * Builds with cmake (see CMakeLists.txt) against the vendored cJSON.
 * FIXDIR is injected by CMake as the absolute path to ./fixtures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "econ_parse.h"

static int g_total = 0, g_fail = 0;

#define CHECK(cond) do { g_total++; if (!(cond)) { g_fail++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_STR(a, b) do { g_total++; if (strcmp((a), (b)) != 0) { g_fail++; \
    printf("  FAIL %s:%d  %s == \"%s\"  got \"%s\"\n", __FILE__, __LINE__, #a, (b), (a)); } } while (0)

#define KST (9L * 3600L)

static char *slurp(const char *name) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", FIXDIR, name);
    FILE *f = fopen(path, "rb");
    if (!f) { printf("cannot open fixture %s\n", path); exit(2); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc(n + 1);
    if (fread(b, 1, n, f) != (size_t)n) { printf("short read %s\n", path); exit(2); }
    b[n] = 0; fclose(f);
    return b;
}

static void test_ymd_to_epoch(void) {
    printf("test_ymd_to_epoch\n");
    CHECK(econ_ymd_to_epoch(1970, 1, 1, 0, 0, 0) == 0);
    CHECK(econ_ymd_to_epoch(2026, 1, 1, 0, 0, 0) == 1767225600);     /* known anchor */
    CHECK(econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0) == 1781611200);
}

static void test_impact_from_str(void) {
    printf("test_impact_from_str\n");
    CHECK(econ_impact_from_str("High")   == ECON_IMPACT_HIGH);
    CHECK(econ_impact_from_str("Medium") == ECON_IMPACT_MEDIUM);
    CHECK(econ_impact_from_str("Low")    == ECON_IMPACT_LOW);
    CHECK(econ_impact_from_str("high")   == ECON_IMPACT_HIGH);   /* case-insensitive */
    CHECK(econ_impact_from_str("None")   == ECON_IMPACT_NONE);
    CHECK(econ_impact_from_str("")       == ECON_IMPACT_NONE);
    CHECK(econ_impact_from_str(NULL)     == ECON_IMPACT_NONE);
}

static void test_week_range(void) {
    printf("test_week_range\n");
    char from[12], to[12], label[24];

    /* 2026-06-16 is a Tuesday; its Mon..Sun week is 06-15 .. 06-21 (UTC). */
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);
    econ_week_range(now, 0, 0, from, sizeof from, to, sizeof to, label, sizeof label);
    CHECK_STR(from, "2026-06-15");
    CHECK_STR(to,   "2026-06-21");
    CHECK_STR(label, "06-15 ~ 06-21");

    econ_week_range(now, 0, -1, from, sizeof from, to, sizeof to, label, sizeof label);
    CHECK_STR(from, "2026-06-08");
    CHECK_STR(to,   "2026-06-14");

    econ_week_range(now, 0, +1, from, sizeof from, to, sizeof to, label, sizeof label);
    CHECK_STR(from, "2026-06-22");
    CHECK_STR(to,   "2026-06-28");

    /* tz boundary: Sun 18:00 UTC is already Mon 03:00 KST -> the *next* week. */
    time_t sun = (time_t)econ_ymd_to_epoch(2026, 6, 14, 18, 0, 0);
    econ_week_range(sun, 0, 0, from, sizeof from, to, sizeof to, label, sizeof label);
    CHECK_STR(from, "2026-06-08");          /* UTC: still last week */
    econ_week_range(sun, KST, 0, from, sizeof from, to, sizeof to, label, sizeof label);
    CHECK_STR(from, "2026-06-15");          /* KST: rolled into this week */
}

static void test_month_week_span(void) {
    printf("test_month_week_span\n");
    int lo = 99, hi = -99;

    /* 2026-06-16 (Tue): June's Mon..Sun weeks span 06-01..07-05, i.e. offsets
     * -2..+2 around the current (06-15 ~ 06-21) week. */
    time_t jun = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);
    econ_month_week_span(jun, 0, &lo, &hi);
    CHECK(lo == -2);
    CHECK(hi == +2);

    /* 2026-07-01 (Wed) sits in the week starting Mon 06-29; that week overlaps
     * July, so the current week is the FIRST of the month (lo == 0) and July
     * runs out to the week of 07-27 (+4). */
    time_t jul = (time_t)econ_ymd_to_epoch(2026, 7, 1, 12, 0, 0);
    econ_month_week_span(jul, 0, &lo, &hi);
    CHECK(lo == 0);
    CHECK(hi == +4);

    /* The current week is always inside the span (now is inside that week). */
    econ_month_week_span(jun, KST, &lo, &hi);
    CHECK(lo <= 0 && hi >= 0);
}

static void test_parse_high_only(void) {
    printf("test_parse_high_only\n");
    char *j = slurp("fmp_econ.json");
    econ_calendar_t c;
    int rc = econ_parse_calendar(j, 0, ECON_IMPACT_HIGH, &c);
    CHECK(rc == 0);
    CHECK(c.valid);
    CHECK(c.count == 3);            /* CPI, BoJ, Fed are High */
    CHECK(c.total_matched == 3);

    /* sorted ascending by time: CPI Mon 06-15, BoJ Wed 06-17, Fed Thu 06-18.
     * The "when" column is a short weekday + HH:MM. */
    CHECK_STR(c.items[0].country, "US");
    CHECK_STR(c.items[0].when, "Mon 12:30");
    CHECK(strncmp(c.items[0].event, "CPI", 3) == 0);
    CHECK(c.items[0].impact == ECON_IMPACT_HIGH);
    CHECK_STR(c.items[0].estimate, "3.20");
    CHECK_STR(c.items[0].actual, "--");        /* null actual -> "--" */
    CHECK_STR(c.items[0].previous, "3.10");

    CHECK_STR(c.items[1].country, "JP");
    CHECK_STR(c.items[1].when, "Wed 03:00");
    CHECK_STR(c.items[1].estimate, "0.75");
    CHECK_STR(c.items[1].actual, "1.00");

    CHECK_STR(c.items[2].country, "US");
    CHECK_STR(c.items[2].when, "Thu 14:00");
    free(j);
}

static void test_parse_tz_shift(void) {
    printf("test_parse_tz_shift\n");
    char *j = slurp("fmp_econ.json");
    econ_calendar_t c;
    CHECK(econ_parse_calendar(j, KST, ECON_IMPACT_HIGH, &c) == 0);
    /* CPI 06-15(Mon) 12:30 UTC -> 21:30 KST same day */
    CHECK_STR(c.items[0].when, "Mon 21:30");
    /* BoJ 06-17(Wed) 03:00 UTC -> 12:00 KST same day */
    CHECK_STR(c.items[1].when, "Wed 12:00");
    free(j);
}

static void test_parse_min_impact(void) {
    printf("test_parse_min_impact\n");
    char *j = slurp("fmp_econ.json");
    econ_calendar_t c;

    CHECK(econ_parse_calendar(j, 0, ECON_IMPACT_MEDIUM, &c) == 0);
    CHECK(c.count == 5);            /* + ZEW + National CPI (Medium) */
    CHECK_STR(c.items[1].country, "DE");      /* ZEW 06-16 sits between CPI and BoJ */

    CHECK(econ_parse_calendar(j, 0, ECON_IMPACT_LOW, &c) == 0);
    CHECK(c.count == 6);            /* + Retail Sales (Low) */
    free(j);
}

static void test_parse_cap(void) {
    printf("test_parse_cap\n");
    /* ECON_EVENT_MAX+3 High events, all on 2026-06-15 (Monday) at strictly
     * increasing times (14-min steps keep them within the day for any
     * ECON_EVENT_MAX): only the earliest ECON_EVENT_MAX are kept, total_matched
     * counts them all. */
    const int extra = 3, total = ECON_EVENT_MAX + extra;
    char buf[16384];
    size_t n = 0;
    n += snprintf(buf + n, sizeof(buf) - n, "[");
    for (int h = 0; h < total; h++) {
        int mins = h * 14;                 /* < 24h for the realistic cap range */
        n += snprintf(buf + n, sizeof(buf) - n,
            "%s{\"date\":\"2026-06-15 %02d:%02d:00\",\"country\":\"US\","
            "\"event\":\"E%d\",\"impact\":\"High\"}", h ? "," : "",
            mins / 60, mins % 60, h);
    }
    n += snprintf(buf + n, sizeof(buf) - n, "]");

    econ_calendar_t c;
    CHECK(econ_parse_calendar(buf, 0, ECON_IMPACT_HIGH, &c) == 0);
    CHECK(c.count == ECON_EVENT_MAX);
    CHECK(c.total_matched == total);
    CHECK_STR(c.items[0].when, "Mon 00:00");                 /* earliest kept */
    char last[ECON_WHEN_MAXLEN];
    int lm = (ECON_EVENT_MAX - 1) * 14;
    snprintf(last, sizeof(last), "Mon %02d:%02d", lm / 60, lm % 60);
    CHECK_STR(c.items[ECON_EVENT_MAX - 1].when, last);       /* last kept */
}

static void test_parse_string_fields(void) {
    printf("test_parse_string_fields\n");
    /* The investing.com proxy keeps units, so estimate/actual/previous arrive as
     * strings ("1.00%") not numbers. Show them verbatim; empty string -> "--". */
    const char *j =
        "[{\"date\":\"2026-06-16 12:00:00\",\"country\":\"JPY\",\"event\":\"BoJ Rate\","
        "\"estimate\":\"1.00%\",\"actual\":\"\",\"previous\":\"0.75%\",\"impact\":\"High\"}]";
    econ_calendar_t c;
    CHECK(econ_parse_calendar(j, 0, ECON_IMPACT_HIGH, &c) == 0);
    CHECK(c.count == 1);
    CHECK_STR(c.items[0].estimate, "1.00%");
    CHECK_STR(c.items[0].actual, "--");        /* empty string -> "--" */
    CHECK_STR(c.items[0].previous, "0.75%");
}

static void test_parse_errors(void) {
    printf("test_parse_errors\n");
    econ_calendar_t c;

    /* empty array: valid, zero events */
    CHECK(econ_parse_calendar("[]", 0, ECON_IMPACT_LOW, &c) == 0);
    CHECK(c.valid);
    CHECK(c.count == 0);

    /* FMP error object -> failure, message surfaced verbatim */
    CHECK(econ_parse_calendar("{\"Error Message\":\"Invalid API KEY.\"}", 0,
                              ECON_IMPACT_LOW, &c) < 0);
    CHECK(!c.valid);
    CHECK(strstr(c.error, "Invalid API KEY") != NULL);

    /* malformed JSON -> failure */
    CHECK(econ_parse_calendar("not json", 0, ECON_IMPACT_LOW, &c) < 0);
    CHECK(!c.valid);
}

int main(void) {
    test_ymd_to_epoch();
    test_impact_from_str();
    test_week_range();
    test_month_week_span();
    test_parse_high_only();
    test_parse_tz_shift();
    test_parse_min_impact();
    test_parse_cap();
    test_parse_string_fields();
    test_parse_errors();
    printf("\n%s  (%d checks, %d failed)\n", g_fail ? "FAILED" : "OK", g_total, g_fail);
    return g_fail ? 1 : 0;
}
