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

/* Build the home page contents under `page` (a full-screen 400x300 container). */
void ui_home_create(lv_obj_t *page);

/* Feed the latest stock quote (symbol / price / change) shown on the card. */
void ui_home_set_quote(const stock_quote_t *q);

/* Feed the latest environment readings (temperature / humidity / battery). */
void ui_home_set_env(const ui_env_t *env);

/* Refresh the clock + date from the system time (already local via TZ).
 * Call on each rotation tick and whenever the home page becomes visible. */
void ui_home_tick(void);

/* Feed the next high-impact economic event into the row below the stock line.
 * when_label is a caller-formatted relative time ("TODAY 21:30"). Pass
 * valid=false (ev/when_label ignored) to clear the row. */
void ui_home_set_econ(const econ_event_t *ev, const char *when_label, bool valid);

#ifdef __cplusplus
}
#endif
