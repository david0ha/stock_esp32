/*
 * ui_stock.h — LVGL UI for the stock monitor (pure LVGL, hardware-agnostic).
 *
 * Layout for the 400x300 reflective mono panel:
 *   - a fixed top bar:  SYMBOL  |  PRICE  |  change-% badge
 *   - a rotating body with 3 pages: [0] intraday chart, [1] news, [2] metrics
 *   - page-indicator dots at the bottom
 *
 * The same source compiles in the desktop simulator and the device firmware.
 * Call create() once under a screen, then update() whenever data changes and
 * show_page() on the rotation timer.
 */
#pragma once

#include "lvgl.h"
#include "stock_model.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_STOCK_PAGE_COUNT 3

void ui_stock_create(lv_obj_t *parent);
void ui_stock_update(const stock_data_t *data);
void ui_stock_show_page(int index);   /* 0..UI_STOCK_PAGE_COUNT-1 */

#ifdef __cplusplus
}
#endif
