/*
 * Host unit tests for stock_api_json (GET /api/info and GET /api/stock/state serializers).
 * Builds with cmake (see CMakeLists.txt). Uses the vendored cJSON to assert the output is
 * valid JSON with the expected fields, plus a couple of exact-string checks.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "stock_api_json.h"
#include "cJSON.h"

static int g_total = 0, g_fail = 0;

#define CHECK(cond) do { g_total++; if (!(cond)) { g_fail++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_STR(a, b) do { g_total++; if (strcmp((a), (b)) != 0) { g_fail++; \
    printf("  FAIL %s:%d  %s == \"%s\"  got \"%s\"\n", __FILE__, __LINE__, #a, (b), (a)); } } while (0)

#define CHECK_DBL(a, b, eps) do { g_total++; double _d = (double)(a) - (double)(b); \
    if (_d < 0) _d = -_d; if (_d > (eps)) { g_fail++; \
    printf("  FAIL %s:%d  %s ~= %s  (%.6f vs %.6f)\n", __FILE__, __LINE__, #a, #b, (double)(a), (double)(b)); } } while (0)

static void test_info(void) {
    printf("test_info\n");
    char out[256];
    int n = stock_api_json_info(out, sizeof(out), "9F3A", "Ticker Board", "0.1.0", "192.168.0.42");
    CHECK(n > 0);
    CHECK_STR(out,
        "{\"deviceId\":\"9F3A\",\"model\":\"Ticker Board\",\"fw\":\"0.1.0\",\"ip\":\"192.168.0.42\"}");

    // NULL fields render as empty strings, still valid JSON.
    stock_api_json_info(out, sizeof(out), NULL, NULL, NULL, NULL);
    CHECK_STR(out, "{\"deviceId\":\"\",\"model\":\"\",\"fw\":\"\",\"ip\":\"\"}");

    // Overflow yields an empty string and -1.
    char tiny[8];
    CHECK(stock_api_json_info(tiny, sizeof(tiny), "9F3A", "m", "f", "i") == -1);
    CHECK_STR(tiny, "");
}

static stock_api_state_t make_state(void) {
    stock_api_state_t st;
    memset(&st, 0, sizeof(st));
    strcpy(st.model, "Ticker Board");
    strcpy(st.fw, "0.1.0");
    strcpy(st.device_id, "9F3A");
    strcpy(st.ip, "192.168.0.42");
    st.index = 1;
    st.page = 2;
    st.econ_mode = true;
    st.econ_week = -1;
    st.refresh_seconds = 30;
    st.keys.finnhub = true;
    st.keys.fmp = false;
    st.keys.econ_url = true;
    st.env.valid = true;
    st.env.temp_c = 24.3;
    st.env.humidity = 41.0;
    st.env.battery_valid = true;
    st.env.battery_v = 4.02;
    st.env.battery_pct = 88;
    st.ticker_count = 2;
    strcpy(st.tickers[0].symbol, "AAPL");
    st.tickers[0].valid = true;
    st.tickers[0].price = 201.5;
    st.tickers[0].change = 1.2;
    st.tickers[0].percent = 0.6;
    st.tickers[0].age_sec = 12;
    strcpy(st.tickers[1].symbol, "TSLA");
    st.tickers[1].valid = false;
    st.tickers[1].age_sec = -1;
    return st;
}

static void test_state_fields(void) {
    printf("test_state_fields\n");
    stock_api_state_t st = make_state();
    char out[2048];
    int n = stock_api_json_state(&st, out, sizeof(out));
    CHECK(n > 0);

    cJSON *root = cJSON_Parse(out);
    CHECK(root != NULL);
    if (!root) return;

    CHECK_STR(cJSON_GetObjectItem(root, "model")->valuestring, "Ticker Board");
    CHECK_STR(cJSON_GetObjectItem(root, "fw")->valuestring, "0.1.0");
    CHECK_STR(cJSON_GetObjectItem(root, "deviceId")->valuestring, "9F3A");
    CHECK_STR(cJSON_GetObjectItem(root, "ip")->valuestring, "192.168.0.42");
    CHECK(cJSON_GetObjectItem(root, "index")->valueint == 1);
    CHECK(cJSON_GetObjectItem(root, "page")->valueint == 2);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(root, "econMode")));
    CHECK(cJSON_GetObjectItem(root, "econWeek")->valueint == -1);
    CHECK(cJSON_GetObjectItem(root, "refreshSeconds")->valueint == 30);

    cJSON *keys = cJSON_GetObjectItem(root, "keys");
    CHECK(keys != NULL);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(keys, "finnhub")));
    CHECK(cJSON_IsFalse(cJSON_GetObjectItem(keys, "fmp")));
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(keys, "econUrl")));

    cJSON *env = cJSON_GetObjectItem(root, "env");
    CHECK(env != NULL);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(env, "valid")));
    CHECK_DBL(cJSON_GetObjectItem(env, "tempC")->valuedouble, 24.3, 1e-6);
    CHECK_DBL(cJSON_GetObjectItem(env, "humidity")->valuedouble, 41.0, 1e-6);
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(env, "batteryValid")));
    CHECK_DBL(cJSON_GetObjectItem(env, "batteryV")->valuedouble, 4.02, 1e-6);
    CHECK(cJSON_GetObjectItem(env, "batteryPct")->valueint == 88);

    cJSON *wl = cJSON_GetObjectItem(root, "watchlist");
    CHECK(cJSON_IsArray(wl));
    CHECK(cJSON_GetArraySize(wl) == 2);
    cJSON *t0 = cJSON_GetArrayItem(wl, 0);
    CHECK_STR(cJSON_GetObjectItem(t0, "symbol")->valuestring, "AAPL");
    CHECK(cJSON_IsTrue(cJSON_GetObjectItem(t0, "valid")));
    CHECK_DBL(cJSON_GetObjectItem(t0, "price")->valuedouble, 201.5, 1e-4);
    CHECK_DBL(cJSON_GetObjectItem(t0, "change")->valuedouble, 1.2, 1e-4);
    CHECK_DBL(cJSON_GetObjectItem(t0, "percent")->valuedouble, 0.6, 1e-4);
    CHECK(cJSON_GetObjectItem(t0, "ageSec")->valueint == 12);
    cJSON *t1 = cJSON_GetArrayItem(wl, 1);
    CHECK_STR(cJSON_GetObjectItem(t1, "symbol")->valuestring, "TSLA");
    CHECK(cJSON_IsFalse(cJSON_GetObjectItem(t1, "valid")));
    CHECK(cJSON_GetObjectItem(t1, "ageSec")->valueint == -1);

    cJSON_Delete(root);
}

static void test_state_empty_watchlist(void) {
    printf("test_state_empty_watchlist\n");
    stock_api_state_t st;
    memset(&st, 0, sizeof(st));
    strcpy(st.model, "Ticker Board");
    st.ticker_count = 0;
    char out[512];
    int n = stock_api_json_state(&st, out, sizeof(out));
    CHECK(n > 0);
    cJSON *root = cJSON_Parse(out);
    CHECK(root != NULL);
    if (root) {
        cJSON *wl = cJSON_GetObjectItem(root, "watchlist");
        CHECK(cJSON_IsArray(wl));
        CHECK(cJSON_GetArraySize(wl) == 0);
        cJSON_Delete(root);
    }
}

static void test_state_non_finite_becomes_zero(void) {
    printf("test_state_non_finite_becomes_zero\n");
    stock_api_state_t st = make_state();
    st.tickers[0].price = NAN;          // a dead endpoint could leave NaN behind
    st.env.temp_c = INFINITY;
    char out[2048];
    int n = stock_api_json_state(&st, out, sizeof(out));
    CHECK(n > 0);
    cJSON *root = cJSON_Parse(out);     // must still be parseable (NaN/Inf are not valid JSON)
    CHECK(root != NULL);
    if (root) {
        cJSON *wl = cJSON_GetObjectItem(root, "watchlist");
        cJSON *t0 = cJSON_GetArrayItem(wl, 0);
        CHECK_DBL(cJSON_GetObjectItem(t0, "price")->valuedouble, 0.0, 1e-9);
        cJSON *env = cJSON_GetObjectItem(root, "env");
        CHECK_DBL(cJSON_GetObjectItem(env, "tempC")->valuedouble, 0.0, 1e-9);
        cJSON_Delete(root);
    }
}

// A JSON number must never end in '.' (e.g. "100.") — strict parsers (JS JSON.parse / Python
// json.loads) reject that even though the lenient vendored cJSON accepts it. Returns true if any
// '.' in `s` is immediately followed by a non-digit (the trailing-dot malformed-number shape).
static int has_trailing_dot_number(const char *s) {
    for (const char *p = s; *p; p++) {
        if (*p == '.') {
            char nxt = p[1];
            if (nxt < '0' || nxt > '9') return 1;
        }
    }
    return 0;
}

static void test_state_huge_finite_stays_valid(void) {
    printf("test_state_huge_finite_stays_valid\n");
    stock_api_state_t st = make_state();
    // Finite but far too large for the fixed format buffers — a garbage/hostile upstream quote.
    // Must NOT produce a truncated "123." number that breaks strict JSON parsers on the phone.
    st.tickers[0].price = 1e20;
    st.tickers[0].change = -1e18;
    st.tickers[0].percent = 1e13;
    st.env.temp_c = 1e13;
    st.env.battery_v = 1e12;
    char out[2048];
    int n = stock_api_json_state(&st, out, sizeof(out));
    CHECK(n > 0);
    CHECK(!has_trailing_dot_number(out));      // strict: no "100." anywhere
    cJSON *root = cJSON_Parse(out);
    CHECK(root != NULL);
    if (root) {
        cJSON *wl = cJSON_GetObjectItem(root, "watchlist");
        cJSON *t0 = cJSON_GetArrayItem(wl, 0);
        CHECK_DBL(cJSON_GetObjectItem(t0, "price")->valuedouble, 0.0, 1e-9);   // overflow -> 0
        cJSON *env = cJSON_GetObjectItem(root, "env");
        CHECK_DBL(cJSON_GetObjectItem(env, "tempC")->valuedouble, 0.0, 1e-9);
        cJSON_Delete(root);
    }
}

static void test_state_truncates_but_stays_valid(void) {
    printf("test_state_truncates_but_stays_valid\n");
    stock_api_state_t st = make_state();
    st.ticker_count = STOCK_API_MAX_TICKERS;
    for (size_t i = 0; i < STOCK_API_MAX_TICKERS; i++) {
        snprintf(st.tickers[i].symbol, sizeof(st.tickers[i].symbol), "SYM%zu", i);
        st.tickers[i].valid = true;
    }
    // Envelope fits but not all 16 tickers -> trailing entries dropped, JSON still valid.
    char out[420];
    int n = stock_api_json_state(&st, out, sizeof(out));
    CHECK(n > 0);
    cJSON *root = cJSON_Parse(out);
    CHECK(root != NULL);
    if (root) {
        CHECK(cJSON_IsArray(cJSON_GetObjectItem(root, "watchlist")));
        cJSON_Delete(root);
    }
}

static void test_state_tight_buffer_fails_clean(void) {
    printf("test_state_tight_buffer_fails_clean\n");
    stock_api_state_t st = make_state();
    char out[16];                       // far too small even for the envelope
    int n = stock_api_json_state(&st, out, sizeof(out));
    CHECK(n == -1);
    CHECK_STR(out, "");
}

int main(void) {
    test_info();
    test_state_fields();
    test_state_empty_watchlist();
    test_state_non_finite_becomes_zero();
    test_state_huge_finite_stays_valid();
    test_state_truncates_but_stays_valid();
    test_state_tight_buffer_fails_clean();
    printf("\n%d/%d checks passed, %d failed\n", g_total - g_fail, g_total, g_fail);
    return g_fail ? 1 : 0;
}
