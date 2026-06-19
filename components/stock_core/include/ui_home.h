/*
 * ui_home.h — the primary "home" screen (clock + date + stock + weather),
 * styled after a bold black-on-white ticker card for the 400x300 mono panel.
 *
 * It is a swappable unit: ui_stock owns the top bar / stock pages / dots and
 * delegates the whole home page to this module. The desktop simulator can
 * compile a different implementation of this file (via the HOME_SRC CMake var)
 * to A/B competing layouts against the reference photo without touching the
 * rest of the UI.
 */
#pragma once

#include "lvgl.h"
#include "stock_model.h"
#include "ui_stock.h"   /* ui_env_t */
#include "econ_model.h" /* econ_event_t */

#ifdef __cplusplus
extern "C" {
#endif

/* A 4-state weather glyph — all the reflective panel can express clearly. */
typedef enum {
    HOME_WX_SUN,
    HOME_WX_PARTLY,
    HOME_WX_CLOUD,
    HOME_WX_RAIN,
} home_wx_t;

/* One sidebar ticker row. */
typedef struct {
    char   symbol[STOCK_SYMBOL_MAXLEN];
    double price;
    double percent;   /* signed day change %  */
    bool   valid;
} home_ticker_t;

/* One forecast column. */
typedef struct {
    char      dow[4];   /* "FRI" */
    home_wx_t wx;
    int       lo;       /* °C low  */
    int       hi;       /* °C high */
} home_forecast_t;

#define HOME_TICKERS_MAX  3
#define HOME_FORECAST_MAX 7
#define HOME_ECON_MAX     3   /* nearest upcoming econ events shown on the home row */

/* Build the home page contents under `page` (a full-screen 400x300 container). */
void ui_home_create(lv_obj_t *page);

/* Feed the latest stock quote (symbol / price / change) shown on the card.
 * Mirrors into the first sidebar ticker so the on-device rotation still feeds
 * the home screen before the full watchlist API is wired. */
void ui_home_set_quote(const stock_quote_t *q);

/* Feed the sidebar watchlist (up to HOME_TICKERS_MAX rows). */
void ui_home_set_tickers(const home_ticker_t *t, int n);

/* Feed the current/outdoor weather shown beside the clock. */
void ui_home_set_weather(home_wx_t wx, int temp_c, const char *city);

/* Feed the multi-day forecast strip (up to HOME_FORECAST_MAX columns). */
void ui_home_set_forecast(const home_forecast_t *days, int n);

/* Feed the latest environment readings (temperature / humidity / battery). */
void ui_home_set_env(const ui_env_t *env);

/* Refresh the clock + date from the system time (already local via TZ).
 * Call on each rotation tick and whenever the home page becomes visible. */
void ui_home_tick(void);

/* Feed the nearest upcoming economic events into the rows below the forecast.
 * `evs`/`when_labels` are parallel arrays of length `n` (0..HOME_ECON_MAX);
 * when_labels[i] is a caller-formatted relative time ("TODAY 21:30"). Pass n=0
 * (evs/when_labels ignored) to clear the rows. */
void ui_home_set_econ(const econ_event_t *evs, const char *const *when_labels, int n);

#ifdef __cplusplus
}
#endif
