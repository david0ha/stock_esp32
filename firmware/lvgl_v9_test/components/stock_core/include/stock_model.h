/*
 * stock_model.h — platform-agnostic data model for the stock monitor.
 *
 * These structs are the single source of truth shared by every layer:
 * parsers (stock_parse) fill them, the service (stock_service) aggregates
 * them, and the LVGL UI (ui_stock) renders them. No ESP-IDF or LVGL types
 * leak in here on purpose, so the exact same model compiles in the desktop
 * simulator, the host unit tests, and the device firmware.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

/* Sizing tuned for a 400x300 monochrome panel + 8MB PSRAM headroom. */
#define STOCK_SYMBOL_MAXLEN     16
#define STOCK_CURRENCY_MAXLEN   8
#define STOCK_CANDLE_MAX        128   /* 5m buckets over a US session ~= 78-100 */
#define STOCK_NEWS_MAX          5     /* headlines shown on the news screen      */
#define STOCK_HEADLINE_MAXLEN   128
#define STOCK_SOURCE_MAXLEN     32

/* Live quote — Finnhub /quote (real-time-ish, no candle access needed). */
typedef struct {
    char    symbol[STOCK_SYMBOL_MAXLEN];
    double  price;       /* c  current price                 */
    double  change;      /* d  absolute change vs prev close */
    double  percent;     /* dp percent change                */
    double  high;        /* h  day high                      */
    double  low;         /* l  day low                       */
    double  open;        /* o  day open                      */
    double  prev_close;  /* pc previous close                */
    int64_t timestamp;   /* t  epoch seconds                 */
    bool    valid;
} stock_quote_t;

/* Intraday line series — Yahoo v8 /chart (range=1d&interval=5m).
 * close[] is forward-filled (gaps carry the last known price) so the
 * polyline never breaks; leading nulls before the first trade are skipped. */
typedef struct {
    char     symbol[STOCK_SYMBOL_MAXLEN];
    char     currency[STOCK_CURRENCY_MAXLEN];
    double   prev_close;                 /* meta.chartPreviousClose */
    int      count;                      /* valid points in close[] */
    float    close[STOCK_CANDLE_MAX];
    int64_t  t_start;                    /* epoch s of first point (exchange UTC) */
    int64_t  t_end;                      /* epoch s of last REAL point (~now intraday) */
    int32_t  gmtoffset;                  /* meta.gmtoffset: exchange offset from UTC (s) */
    double   day_min;                    /* min(close) — chart Y range */
    double   day_max;                    /* max(close) — chart Y range */
    double   day_high;                   /* max(high[]) — true intraday high */
    double   day_low;                    /* min(low[])  — true intraday low  */
    bool     valid;
} stock_series_t;

/* Fundamentals — Finnhub /stock/metric?metric=all. */
typedef struct {
    char    symbol[STOCK_SYMBOL_MAXLEN];
    double  pe_ttm;        /* peTTM                          */
    double  eps_ttm;       /* epsTTM                         */
    double  market_cap;    /* marketCapitalization (millions)*/
    double  week52_high;   /* "52WeekHigh"                   */
    double  week52_low;    /* "52WeekLow"                    */
    double  div_yield;     /* dividendYieldIndicatedAnnual(%)*/
    double  beta;          /* beta                           */
    bool    valid;
} stock_metrics_t;

/* News — Finnhub /company-news. */
typedef struct {
    char     headline[STOCK_HEADLINE_MAXLEN];
    char     source[STOCK_SOURCE_MAXLEN];
    int64_t  datetime;     /* epoch seconds */
} stock_news_item_t;

typedef struct {
    int                count;
    stock_news_item_t  items[STOCK_NEWS_MAX];
    bool               valid;
} stock_news_t;

/* Aggregate snapshot the UI renders. Each sub-struct carries its own
 * `valid` flag so a single failing endpoint degrades gracefully instead
 * of blanking the whole screen. */
typedef struct {
    stock_quote_t   quote;
    stock_series_t  series;
    stock_metrics_t metrics;
    stock_news_t    news;
} stock_data_t;
