/*
 * user_app_control.h — thread-safe control bridge for the companion-app HTTP server.
 *
 * The stock app's UI state (on-screen ticker, page, economic-calendar overlay, watchlist) is
 * normally driven by the physical USER/BOOT buttons on the StockTask. This bridge lets the
 * STA-mode HTTP server (components/stock_api) drive the SAME actions and read a live snapshot,
 * without ever touching LVGL or the app state directly:
 *
 *   - reads  (user_app_snapshot)         take the app's state lock and copy out plain data.
 *   - writes (select/page/econ/refresh/…) validate cheaply, then post a command onto the app's
 *     command queue; StockTask applies it on the UI task via the same code path as a button
 *     press, so there is no cross-task LVGL access and no new race.
 *
 * All functions are safe to call from the HTTP server task. They are no-ops (returning false /
 * an empty snapshot) until UserApp_TaskInit has created the queues.
 */
#pragma once

#include <stdbool.h>

#include "stock_api_model.h"   /* stock_api_state_t — the snapshot shape */

/* Identity reported to the app. model/fw are filled into the snapshot by user_app; the network
 * identity (device_id, ip) is filled by the stock_api layer which owns esp_netif/esp_mac. */
#define STOCK_APP_MODEL  "Ticker Board"
#define STOCK_APP_FW     "0.1.0"

#ifdef __cplusplus
extern "C" {
#endif

/* Fill `out` with a snapshot of the running app (model/fw/index/page/econ/env/watchlist).
 * Leaves out->device_id and out->ip empty for the caller to populate. Thread-safe. */
void user_app_snapshot(stock_api_state_t *out);

/* Select the on-screen ticker by watchlist index. Returns false if index is out of range. */
bool user_app_select_index(int index);

/* Resolve `symbol` (case-insensitive, normalized like the watchlist) to an index and select it.
 * Returns false if the symbol is not in the current watchlist. */
bool user_app_select_symbol(const char *symbol);

/* Switch the view page (0=home,1=chart,2=news,3=metrics). Returns false if out of range. */
bool user_app_set_page(int page);

/* Show/hide the economic-calendar overlay; `week` (0=this,-1=prev,+1=next) applies when shown. */
void user_app_set_econ(bool mode, int week);

/* Force a data refresh of the on-screen ticker (all=false) or the whole watchlist (all=true). */
void user_app_refresh(bool all);

/* Replace the watchlist from a comma/space-separated list (normalized by prov_tickers_parse),
 * persisted to NVS and applied live with no reboot. Returns the resulting ticker count, or 0 if
 * the list parsed empty (rejected — the watchlist is left unchanged). */
int user_app_set_watchlist(const char *csv);

/* Update the runtime data-source keys/URL, persisted to NVS and applied live (a re-fetch is
 * triggered). Each argument is updated only if non-NULL (an empty string clears that key, falling
 * back to the compile-time Kconfig default); pass NULL to leave a field unchanged. Returns true if
 * at least one field was provided and the update was enqueued. */
bool user_app_set_keys(const char *finnhub, const char *fmp, const char *econ_url);

/* Set the weather location (free text, e.g. "Seoul"), persisted to NVS and applied live — the
 * device re-geocodes it (Open-Meteo) without a reboot. An empty string turns the weather widget
 * off. Returns true if the change was enqueued. */
bool user_app_set_location(const char *place);

#ifdef __cplusplus
}
#endif
