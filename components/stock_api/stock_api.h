/*
 * stock_api.h — companion-app control server (STA mode).
 *
 * Once the device is on the home Wi-Fi, this brings up a small HTTP/JSON server (port 80) and
 * advertises it over mDNS as "tickerboard.local" so the phone app can read a live snapshot of
 * the running stock app and drive it (watchlist / on-screen ticker / page / econ overlay /
 * refresh). All control flows through the user_app_control bridge, so this server never touches
 * LVGL or the app state directly. Call once, after the network is up and UserApp_TaskInit ran.
 *
 * Endpoints (see docs/app-control.md for the full contract):
 *   GET  /api/info                 -> { deviceId, model, fw, ip }
 *   GET  /api/stock/state          -> live snapshot
 *   POST /api/stock/select         { index | symbol }
 *   POST /api/stock/page           { page }
 *   POST /api/stock/econ           { mode, week? }
 *   POST /api/stock/refresh        { all? }
 *   POST /api/stock/watchlist      { tickers: [..] | "AAPL,TSLA" }
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void stock_api_start(void);

#ifdef __cplusplus
}
#endif
