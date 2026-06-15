/*
 * Host unit tests for the stock_service layer.
 *
 * stock_service talks to the network only through the http_get() seam, so we
 * link a fake http_get here (no sockets) and drive stock_service_fetch_quote
 * against canned responses. Builds with cmake (see CMakeLists.txt).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stock_service.h"
#include "http_port.h"

static int g_total = 0, g_fail = 0;

#define CHECK(cond) do { g_total++; if (!(cond)) { g_fail++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_DBL(a, b, eps) do { g_total++; double _d = (double)(a) - (double)(b); \
    if (_d < 0) _d = -_d; if (_d > (eps)) { g_fail++; \
    printf("  FAIL %s:%d  %s ~= %s  (%.6f vs %.6f)\n", __FILE__, __LINE__, #a, #b, (double)(a), (double)(b)); } } while (0)

/* ---- fake http_get: returns a copy of the configured body + status ---- */
static const char *g_next_body;
static int         g_next_status;

char *http_get(const char *url, int *out_status) {
    (void)url;
    if (out_status) *out_status = g_next_status;
    if (!g_next_body) return NULL;
    return strdup(g_next_body);   /* caller frees, matching the real port */
}

static const char *VALID_QUOTE =
    "{\"c\":310.50,\"d\":5.0,\"dp\":1.64,\"h\":312.0,\"l\":300.0,"
    "\"o\":305.0,\"pc\":305.5,\"t\":1781294400}";

/* A previously-good cached quote we must not lose on a bad refresh. */
static void seed_good_quote(stock_data_t *d) {
    memset(d, 0, sizeof(*d));
    d->quote.valid = true;
    strcpy(d->quote.symbol, "AAPL");
    d->quote.price   = 296.44;
    d->quote.percent = 1.82;
}

static void test_fetch_quote(void) {
    printf("test_fetch_quote\n");

    /* success: a good 200 body updates the quote and returns 1 */
    stock_data_t d;
    seed_good_quote(&d);
    g_next_body = VALID_QUOTE; g_next_status = 200;
    int rc = stock_service_fetch_quote("AAPL", "k", &d);
    CHECK(rc == 1);
    CHECK(d.quote.valid);
    CHECK_DBL(d.quote.price, 310.50, 1e-4);

    /* REGRESSION: a 200 with an unparseable body (Finnhub free-tier rate-limit)
     * must NOT clobber the previously-good cached quote. */
    seed_good_quote(&d);
    g_next_body = "{}"; g_next_status = 200;   /* parses, but no "c" price */
    rc = stock_service_fetch_quote("AAPL", "k", &d);
    CHECK(rc == 0);
    CHECK(d.quote.valid);                       /* still valid */
    CHECK_DBL(d.quote.price, 296.44, 1e-4);     /* price preserved, not zeroed */

    /* a transport failure (NULL body) must likewise preserve the cache */
    seed_good_quote(&d);
    g_next_body = NULL; g_next_status = 0;
    rc = stock_service_fetch_quote("AAPL", "k", &d);
    CHECK(rc == 0);
    CHECK_DBL(d.quote.price, 296.44, 1e-4);

    /* a non-200 status (e.g. 429) with a body must also preserve the cache */
    seed_good_quote(&d);
    g_next_body = VALID_QUOTE; g_next_status = 429;
    rc = stock_service_fetch_quote("AAPL", "k", &d);
    CHECK(rc == 0);
    CHECK_DBL(d.quote.price, 296.44, 1e-4);
}

int main(void) {
    test_fetch_quote();
    printf("\n%s  (%d checks, %d failed)\n", g_fail ? "FAILED" : "OK", g_total, g_fail);
    return g_fail ? 1 : 0;
}
