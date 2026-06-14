/*
 * Host unit tests for the stock_parse layer.
 * Builds with cmake (see CMakeLists.txt) against the vendored cJSON.
 * FIXDIR is injected by CMake as the absolute path to ./fixtures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stock_parse.h"

static int g_total = 0, g_fail = 0;

#define CHECK(cond) do { g_total++; if (!(cond)) { g_fail++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_DBL(a, b, eps) do { g_total++; double _d = (double)(a) - (double)(b); \
    if (_d < 0) _d = -_d; if (_d > (eps)) { g_fail++; \
    printf("  FAIL %s:%d  %s ~= %s  (%.6f vs %.6f)\n", __FILE__, __LINE__, #a, #b, (double)(a), (double)(b)); } } while (0)

#define CHECK_STR(a, b) do { g_total++; if (strcmp((a), (b)) != 0) { g_fail++; \
    printf("  FAIL %s:%d  %s == \"%s\"  got \"%s\"\n", __FILE__, __LINE__, #a, (b), (a)); } } while (0)

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

static void test_quote(void) {
    printf("test_quote\n");
    char *j = slurp("fh_quote.json");
    stock_quote_t q;
    int rc = stock_parse_quote(j, "AAPL", &q);
    CHECK(rc == 0);
    CHECK(q.valid);
    CHECK_STR(q.symbol, "AAPL");
    CHECK_DBL(q.price, 291.15, 1e-4);
    CHECK_DBL(q.change, -4.48, 1e-4);
    CHECK_DBL(q.percent, -1.5154, 1e-4);
    CHECK_DBL(q.high, 297.14, 1e-4);
    CHECK_DBL(q.low, 289.62, 1e-4);
    CHECK_DBL(q.open, 296.04, 1e-2);
    CHECK_DBL(q.prev_close, 295.63, 1e-4);
    CHECK(q.timestamp == 1781294400);
    free(j);

    /* error path: garbage / empty object => invalid, negative rc */
    stock_quote_t bad;
    CHECK(stock_parse_quote("{}", "AAPL", &bad) < 0);
    CHECK(!bad.valid);
    CHECK(stock_parse_quote("not json", "AAPL", &bad) < 0);

    /* non-finite price (e.g. 1e400 -> +Inf via strtod) must be rejected, not
     * propagated to the UI as "inf" */
    stock_quote_t inf;
    CHECK(stock_parse_quote("{\"c\":1e400,\"pc\":100}", "X", &inf) < 0);
    CHECK(!inf.valid);
}

static void test_series(void) {
    printf("test_series\n");
    char *j = slurp("yf_chart.json");
    stock_series_t s;
    int rc = stock_parse_series(j, &s);
    CHECK(rc == 0);
    CHECK(s.valid);
    CHECK_STR(s.symbol, "AAPL");
    CHECK_STR(s.currency, "USD");
    CHECK_DBL(s.prev_close, 295.63, 1e-4);
    CHECK(s.count == 8);
    CHECK_DBL(s.close[0], 290.0, 1e-3);
    CHECK_DBL(s.close[2], 291.5, 1e-3);   /* null forward-filled from close[1] */
    CHECK_DBL(s.close[7], 291.15, 1e-3);
    CHECK_DBL(s.day_min, 289.0, 1e-3);
    CHECK_DBL(s.day_max, 294.0, 1e-3);
    CHECK(s.t_start == 1781290800);
    CHECK(s.t_end == 1781292900);
    CHECK(s.gmtoffset == -14400);
    /* true intraday H/L from high[]/low[] arrays, not the close range */
    CHECK_DBL(s.day_high, 294.3, 1e-3);
    CHECK_DBL(s.day_low, 288.7, 1e-3);
    free(j);

    /* a real close paired with a null/missing timestamp is skipped so it
     * cannot poison t_start/t_end with epoch 0 */
    const char *badts =
        "{\"chart\":{\"result\":[{\"meta\":{\"symbol\":\"Q\",\"currency\":\"USD\","
        "\"chartPreviousClose\":9.0},\"timestamp\":[100,null,300],"
        "\"indicators\":{\"quote\":[{\"close\":[10.0,11.0,12.0]}]}}],\"error\":null}}";
    stock_series_t s4;
    CHECK(stock_parse_series(badts, &s4) == 0);
    CHECK(s4.count == 2);
    CHECK(s4.t_start == 100);
    CHECK(s4.t_end == 300);
    CHECK_DBL(s4.close[1], 12.0, 1e-3);

    /* trailing nulls (market still open) are dropped so the line ends at the
     * last real trade, not flat-filled to the close */
    const char *trail =
        "{\"chart\":{\"result\":[{\"meta\":{\"symbol\":\"Z\",\"currency\":\"USD\","
        "\"chartPreviousClose\":9.0},\"timestamp\":[100,200,300,400,500],"
        "\"indicators\":{\"quote\":[{\"close\":[10.0,11.0,12.0,null,null]}]}}],\"error\":null}}";
    stock_series_t s3;
    CHECK(stock_parse_series(trail, &s3) == 0);
    CHECK(s3.count == 3);
    CHECK(s3.t_end == 300);
    CHECK_DBL(s3.close[2], 12.0, 1e-3);

    /* leading nulls are skipped until the first real trade */
    const char *lead =
        "{\"chart\":{\"result\":[{\"meta\":{\"symbol\":\"X\",\"currency\":\"USD\","
        "\"chartPreviousClose\":10.0},\"timestamp\":[1,2,3],"
        "\"indicators\":{\"quote\":[{\"close\":[null,12.0,13.0]}]}}],\"error\":null}}";
    stock_series_t s2;
    CHECK(stock_parse_series(lead, &s2) == 0);
    CHECK(s2.count == 2);
    CHECK_DBL(s2.close[0], 12.0, 1e-3);
    CHECK_DBL(s2.close[1], 13.0, 1e-3);

    /* error path */
    stock_series_t bad;
    CHECK(stock_parse_series("{\"chart\":{\"result\":null,\"error\":\"Not Found\"}}", &bad) < 0);
    CHECK(!bad.valid);
}

static void test_metrics(void) {
    printf("test_metrics\n");
    char *j = slurp("fh_metric.json");
    stock_metrics_t m;
    int rc = stock_parse_metrics(j, "AAPL", &m);
    CHECK(rc == 0);
    CHECK(m.valid);
    CHECK_STR(m.symbol, "AAPL");
    CHECK_DBL(m.pe_ttm, 34.8159, 1e-3);
    CHECK_DBL(m.eps_ttm, 8.2666, 1e-3);
    CHECK_DBL(m.market_cap, 4267558.5, 1e-1);
    CHECK_DBL(m.week52_high, 317.4, 1e-3);
    CHECK_DBL(m.week52_low, 195.07, 1e-3);
    CHECK_DBL(m.div_yield, 0.36532, 1e-4);
    CHECK_DBL(m.beta, 1.1023339, 1e-4);
    free(j);

    stock_metrics_t bad;
    CHECK(stock_parse_metrics("{}", "AAPL", &bad) < 0);
}

static void test_news(void) {
    printf("test_news\n");
    char *j = slurp("fh_news.json");
    stock_news_t n;
    int rc = stock_parse_news(j, &n);
    CHECK(rc == 0);
    CHECK(n.valid);
    CHECK(n.count == 5);                 /* fixture has exactly STOCK_NEWS_MAX */
    CHECK_STR(n.items[0].source, "Yahoo");
    CHECK(n.items[0].datetime == 1781365360);
    CHECK(strncmp(n.items[0].headline, "Wall Street", 11) == 0);
    /* long headline must be truncated to fit the buffer */
    CHECK(strlen(n.items[4].headline) < STOCK_HEADLINE_MAXLEN);
    free(j);

    /* non-ASCII punctuation is folded to ASCII so the built-in mono font
     * (which lacks curly quotes / dashes) renders no tofu boxes */
    const char *uni =
        "[{\"datetime\":1,\"source\":\"X\","
        "\"headline\":\"Apple\\u2019s \\u201cAI\\u201d \\u2014 era\\u2026\"}]";
    stock_news_t u;
    CHECK(stock_parse_news(uni, &u) == 0);
    CHECK(u.count == 1);
    CHECK_STR(u.items[0].headline, "Apple's \"AI\" - era...");

    /* truncated trailing UTF-8 lead bytes must not over-read past NUL
     * (regression: ASan would fault here before the bounds guard) */
    stock_news_t t;
    CHECK(stock_parse_news("[{\"datetime\":1,\"source\":\"X\",\"headline\":\"hi\xE2\x80\"}]", &t) == 0);
    CHECK(t.count == 1);
    CHECK_STR(t.items[0].headline, "hi");

    /* empty array => valid but zero items */
    stock_news_t empty;
    CHECK(stock_parse_news("[]", &empty) == 0);
    CHECK(empty.count == 0);

    stock_news_t bad;
    CHECK(stock_parse_news("not json", &bad) < 0);
}

int main(void) {
    test_quote();
    test_series();
    test_metrics();
    test_news();
    printf("\n%d/%d checks passed, %d failed\n", g_total - g_fail, g_total, g_fail);
    return g_fail ? 1 : 0;
}
