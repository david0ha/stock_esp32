/*
 * stock_api_model.h — platform-agnostic snapshot of the running stock app, as exposed to the
 * companion phone app over GET /api/stock/state.
 *
 * Like stock_model.h this is intentionally self-contained (no ESP-IDF / LVGL, and no
 * provisioning dependency) so the serializer compiles in the host tests and desktop simulator.
 * user_app fills it under its state lock from prov_config + the live cache; stock_api_json.c
 * serializes it. The ticker cap matches PROV_MAX_TICKERS / PROV_TICKER_MAX_LEN by value.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define STOCK_API_MAX_TICKERS    16   /* == PROV_MAX_TICKERS    */
#define STOCK_API_SYMBOL_MAXLEN  12   /* == PROV_TICKER_MAX_LEN */
#define STOCK_API_MODEL_MAXLEN   24
#define STOCK_API_FW_MAXLEN      16
#define STOCK_API_DEVID_MAXLEN   16
#define STOCK_API_IP_MAXLEN      16
#define STOCK_API_LOCATION_MAXLEN 48   /* == PROV_LOCATION_MAX_LEN  */
#define STOCK_API_CITY_MAXLEN    64    /* resolved "City, CC"       */

/* One watchlist slot's realtime quote, mirrored from the on-device cache. */
typedef struct {
    char   symbol[STOCK_API_SYMBOL_MAXLEN + 1];
    bool   valid;       /* false until the slot has been fetched at least once */
    double price;
    double change;
    double percent;
    int    age_sec;     /* seconds since last fetch; -1 if never fetched       */
} stock_api_ticker_t;

/* Which runtime data-source keys/URL are set (presence only — the secret values are never
 * serialized). Lets the app's settings show "key set / not set" without exposing the key. */
typedef struct {
    bool finnhub;
    bool fmp;
    bool econ_url;
} stock_api_keys_t;

/* Resolved outdoor weather for the configured location (Open-Meteo), mirrored from WeatherTask.
 * `city` is the geocoded "City, CC" confirmation; valid=false until the first forecast lands. */
typedef struct {
    bool valid;
    int  temp_c;
    char city[STOCK_API_CITY_MAXLEN];
} stock_api_weather_t;

/* On-board environment, mirrored from the home page's sensor read. */
typedef struct {
    bool   valid;          /* SHTC3 read succeeded                 */
    double temp_c;
    double humidity;
    bool   battery_valid;  /* battery ADC produced a sane voltage  */
    double battery_v;
    int    battery_pct;
} stock_api_env_t;

/* Full snapshot serialized by GET /api/stock/state. */
typedef struct {
    char   model[STOCK_API_MODEL_MAXLEN];
    char   fw[STOCK_API_FW_MAXLEN];
    char   device_id[STOCK_API_DEVID_MAXLEN];
    char   ip[STOCK_API_IP_MAXLEN];

    int    index;            /* on-screen watchlist index            */
    int    page;             /* 0=home 1=chart 2=news 3=metrics      */
    bool   econ_mode;        /* economic-calendar overlay shown      */
    int    econ_week;        /* 0=this week, -1=prev, +1=next        */
    int    refresh_seconds;  /* background refresh cadence           */

    stock_api_keys_t   keys;
    char               location[STOCK_API_LOCATION_MAXLEN + 1];  /* configured place (city name) */
    stock_api_weather_t weather;
    stock_api_env_t    env;
    size_t             ticker_count;
    stock_api_ticker_t tickers[STOCK_API_MAX_TICKERS];
} stock_api_state_t;
