/*
 * stock_service.c — builds endpoint URLs, fetches via the http_get port,
 * and parses into the model. Shared by firmware and simulator.
 */
#include "stock_service.h"
#include "stock_parse.h"
#include "http_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define URL_MAX 320

/* Fetch a URL; return the body on HTTP 200, else NULL. Always frees on
 * failure so callers only free a non-NULL return. */
static char *fetch_body(const char *url) {
    int st = 0;
    char *body = http_get(url, &st);
    if (body && st == 200) return body;
    free(body);
    return NULL;
}

int stock_service_fetch(const char *symbol, const char *finnhub_key,
                        stock_data_t *out) {
    memset(out, 0, sizeof(*out));
    int ok = 0;
    char url[URL_MAX];
    char *b;

    /* Finnhub quote — price / change / % */
    snprintf(url, sizeof(url),
             "https://finnhub.io/api/v1/quote?symbol=%s&token=%s",
             symbol, finnhub_key);
    if ((b = fetch_body(url))) {
        if (stock_parse_quote(b, symbol, &out->quote) == 0) ok++;
        free(b);
    }

    /* Yahoo v8 chart — intraday 5m line */
    snprintf(url, sizeof(url),
             "https://query1.finance.yahoo.com/v8/finance/chart/%s?range=1d&interval=5m",
             symbol);
    if ((b = fetch_body(url))) {
        if (stock_parse_series(b, &out->series) == 0) ok++;
        free(b);
    }

    /* Finnhub metric — fundamentals */
    snprintf(url, sizeof(url),
             "https://finnhub.io/api/v1/stock/metric?symbol=%s&metric=all&token=%s",
             symbol, finnhub_key);
    if ((b = fetch_body(url))) {
        if (stock_parse_metrics(b, symbol, &out->metrics) == 0) ok++;
        free(b);
    }

    /* Finnhub company-news — last 7 days. Skip entirely if the clock isn't set
     * yet (before SNTP, time(NULL) is ~1970 and the query window is garbage). */
    time_t now = time(NULL);
    if (now > 1700000000) {                 /* ~2023-11; sane clock */
        time_t from = now - 7 * 24 * 3600;
        struct tm tnow, tfrom;
        if (gmtime_r(&now, &tnow) && gmtime_r(&from, &tfrom)) {
            char sfrom[16], sto[16];
            strftime(sfrom, sizeof(sfrom), "%Y-%m-%d", &tfrom);
            strftime(sto, sizeof(sto), "%Y-%m-%d", &tnow);
            snprintf(url, sizeof(url),
                     "https://finnhub.io/api/v1/company-news?symbol=%s&from=%s&to=%s&token=%s",
                     symbol, sfrom, sto, finnhub_key);
            if ((b = fetch_body(url))) {
                if (stock_parse_news(b, &out->news) == 0) ok++;
                free(b);
            }
        }
    }

    return ok;
}
