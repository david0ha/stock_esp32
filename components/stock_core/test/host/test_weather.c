/*
 * Host unit tests for the weather parse layer (Open-Meteo geocoding + forecast).
 * Builds with cmake (see CMakeLists.txt) against the vendored cJSON.
 * FIXDIR is injected by CMake as the absolute path to ./fixtures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "weather_parse.h"

static int g_total = 0, g_fail = 0;

#define CHECK(cond) do { g_total++; if (!(cond)) { g_fail++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_STR(a, b) do { g_total++; if (strcmp((a), (b)) != 0) { g_fail++; \
    printf("  FAIL %s:%d  %s == \"%s\"  got \"%s\"\n", __FILE__, __LINE__, #a, (b), (a)); } } while (0)

#define CHECK_DBL(a, b, eps) do { g_total++; double _d = (double)(a) - (double)(b); \
    if (_d < 0) _d = -_d; if (_d > (eps)) { g_fail++; \
    printf("  FAIL %s:%d  %s ~= %s  (%.6f vs %.6f)\n", __FILE__, __LINE__, #a, #b, (double)(a), (double)(b)); } } while (0)

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

static void test_wmo_mapping(void) {
    printf("test_wmo_mapping\n");
    CHECK(wx_from_wmo(0)  == WX_SUN);
    CHECK(wx_from_wmo(1)  == WX_PARTLY);
    CHECK(wx_from_wmo(2)  == WX_PARTLY);
    CHECK(wx_from_wmo(3)  == WX_CLOUD);
    CHECK(wx_from_wmo(45) == WX_CLOUD);
    CHECK(wx_from_wmo(48) == WX_CLOUD);
    CHECK(wx_from_wmo(51) == WX_RAIN);
    CHECK(wx_from_wmo(61) == WX_RAIN);
    CHECK(wx_from_wmo(71) == WX_RAIN);   /* snow collapses onto rain */
    CHECK(wx_from_wmo(80) == WX_RAIN);
    CHECK(wx_from_wmo(95) == WX_RAIN);
    CHECK(wx_from_wmo(99) == WX_RAIN);
    CHECK(wx_from_wmo(123) == WX_CLOUD); /* unknown -> cloud */
}

static void test_geocode(void) {
    printf("test_geocode\n");
    char *j = slurp("om_geo.json");
    geo_loc_t g;
    int rc = geo_parse(j, &g);
    CHECK(rc == 0);
    CHECK(g.valid);
    CHECK_STR(g.name, "Seoul");
    CHECK_STR(g.country, "KR");
    CHECK_DBL(g.lat, 37.566, 1e-3);   /* first result wins */
    CHECK_DBL(g.lon, 126.9784, 1e-3);
    free(j);

    /* no results -> invalid */
    geo_loc_t e;
    CHECK(geo_parse("{\"generationtime_ms\":0.1}", &e) == -1);
    CHECK(!e.valid);

    /* malformed JSON -> invalid, no crash */
    CHECK(geo_parse("not json", &e) == -1);
    CHECK(geo_parse(NULL, &e) == -1);
}

static void test_forecast(void) {
    printf("test_forecast\n");
    char *j = slurp("om_forecast.json");
    weather_t w;
    int rc = weather_parse(j, &w);
    CHECK(rc == 0);
    CHECK(w.valid);

    /* current */
    CHECK(w.now_valid);
    CHECK(w.now_temp_c == 24);          /* 24.3 rounds to 24 */
    CHECK(w.now_wx == WX_PARTLY);       /* code 2 */

    /* daily: 7 columns, weekday derived from each date */
    CHECK(w.day_count == 7);
    CHECK_STR(w.days[0].dow, "FRI");    /* 2026-06-19 */
    CHECK_STR(w.days[1].dow, "SAT");
    CHECK_STR(w.days[2].dow, "SUN");
    CHECK_STR(w.days[6].dow, "THU");    /* 2026-06-25 */

    CHECK(w.days[0].wx == WX_SUN);      /* code 0 */
    CHECK(w.days[1].wx == WX_PARTLY);   /* code 1 */
    CHECK(w.days[2].wx == WX_CLOUD);    /* code 3 */
    CHECK(w.days[3].wx == WX_CLOUD);    /* code 45 (fog) */
    CHECK(w.days[4].wx == WX_RAIN);     /* code 61 */
    CHECK(w.days[6].wx == WX_RAIN);     /* code 95 (thunderstorm) */

    CHECK(w.days[0].hi == 27);          /* 27.4 -> 27 */
    CHECK(w.days[0].lo == 18);          /* 18.2 -> 18 */
    free(j);

    /* malformed -> invalid */
    weather_t bad;
    CHECK(weather_parse("not json", &bad) == -1);
    CHECK(!bad.valid);
    CHECK(weather_parse(NULL, &bad) == -1);
}

int main(void) {
    test_wmo_mapping();
    test_geocode();
    test_forecast();
    printf("\n%s  (%d checks, %d failed)\n", g_fail ? "FAILED" : "OK", g_total, g_fail);
    return g_fail ? 1 : 0;
}
