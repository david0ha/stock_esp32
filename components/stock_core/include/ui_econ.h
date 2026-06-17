/*
 * ui_econ.h — LVGL view for the FMP economic calendar (pure LVGL, board-agnostic).
 *
 * A full-screen overlay on the 400x300 mono panel, hidden until the user enters
 * the calendar (KEY+BOOT). One screen shows one Mon..Sun week as a chronological
 * list of events with importance stars + estimate/actual; KEY/BOOT page weeks.
 *
 * The same source compiles in the desktop simulator and the device firmware.
 * Call create() once under the screen, then show()/set_loading()/set_calendar().
 */
#pragma once

#include "lvgl.h"
#include "econ_model.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_econ_create(lv_obj_t *parent);
void ui_econ_show(bool show);                          /* overlay on/off          */
void ui_econ_set_loading(const char *week_label);      /* "Loading..." placeholder */
/* Render one page of the week: events [page*ECON_PAGE_MAX .. +ECON_PAGE_MAX) (or
 * the error/empty message). The footer shows the page number within the week. */
void ui_econ_set_calendar(const econ_calendar_t *cal, int page);

#ifdef __cplusplus
}
#endif
