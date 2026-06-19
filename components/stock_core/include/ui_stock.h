/*
 * ui_stock.h — LVGL UI for the stock monitor (pure LVGL, hardware-agnostic).
 *
 * Layout for the 400x300 reflective mono panel:
 *   - a fixed top bar:  SYMBOL | battery  |  PRICE  |  change-% badge
 *   - a rotating body with 4 pages:
 *       [0] home (big clock + date + temperature/humidity),
 *       [1] intraday chart, [2] news, [3] metrics
 *   - page-indicator dots at the bottom
 *
 * The home page is the primary screen — the app's rotation returns to it between
 * each stock page so the clock/weather stays prominent.
 *
 * The same source compiles in the desktop simulator and the device firmware.
 * Call create() once under a screen, then update()/update_env() whenever data
 * changes and show_page() on the rotation timer.
 */
#pragma once

#include "lvgl.h"
#include "stock_model.h"
#include "econ_model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_STOCK_PAGE_COUNT 4
#define UI_STOCK_PAGE_HOME  0   /* index of the time/weather page */

/* Environment readings for the home page + battery indicator. Kept out of
 * stock_model.h so the portable parsers/host-tests stay free of board state. */
typedef struct {
    bool  env_valid;      /* temperature/humidity reading is good */
    float temp_c;
    float humidity;       /* %RH */
    bool  battery_valid;  /* battery voltage reading is good */
    int   battery_pct;    /* 0..100 */
    float battery_v;
} ui_env_t;

void ui_stock_create(lv_obj_t *parent);
void ui_stock_update(const stock_data_t *data);     /* stock data -> top bar + pages 1..3 */
void ui_stock_update_env(const ui_env_t *env);      /* sensors -> home page + battery chip */
void ui_stock_update_econ(const econ_event_t *evs, const char *const *when_labels, int n);
void ui_stock_show_page(int index);                 /* 0..UI_STOCK_PAGE_COUNT-1 */

#ifdef __cplusplus
}
#endif
