/*
 * stock_parse.h — pure JSON -> model parsers.
 *
 * Every function takes a NUL-terminated JSON string and fills the out struct.
 * They have no I/O and no global state, so they are unit-tested on the host
 * against captured API fixtures (see components/stock_core/test/host).
 *
 * Return value: 0 on success, negative on parse error. On error the out
 * struct is zeroed and out->valid is false. On success out->valid is true.
 */
#pragma once

#include "stock_model.h"

/* Finnhub /quote — response has no symbol field, so it is passed in. */
int stock_parse_quote(const char *json, const char *symbol, stock_quote_t *out);

/* Yahoo v8 /chart — symbol/currency/prev_close come from meta. */
int stock_parse_series(const char *json, stock_series_t *out);

/* Finnhub /stock/metric?metric=all — symbol passed in for robustness. */
int stock_parse_metrics(const char *json, const char *symbol, stock_metrics_t *out);

/* Finnhub /company-news — array of articles, newest first. Keeps up to
 * STOCK_NEWS_MAX items; long headlines are truncated to fit. */
int stock_parse_news(const char *json, stock_news_t *out);
