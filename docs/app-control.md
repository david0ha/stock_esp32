# Companion App + ESP32 Control API (App Control)

This document defines the **local-only** (no cloud) HTTP/JSON contract between the
React Native companion app and the firmware. The app communicates with the board in two phases.

```
[1] Onboarding (SoftAP)        [2] Control (home LAN, STA)
 phone ── join AP ──► ESP32     phone ──┐
   http://192.168.4.1            (same Wi-Fi)
   /api/info  /api/scan          └─► http://tickerboard.local  (mDNS)  or  http://<device-ip>
   /api/provision /api/status        /api/info  /api/stock/*  /api/econ
```

- Because the board has a single radio, once onboarding completes the board brings down the SoftAP
  and reboots into STA (home Wi-Fi) mode. From then on, "live control" happens over the home LAN
  (mDNS `tickerboard.local`, with manual IP as fallback).
- All credentials/watchlists are stored only in NVS and are never committed to the repository.

## Design Sources (reference repos)

- `~/Documents/masterham-esp32` — the same-lineage `provisioning` component already has a `/api/*`
  provisioning JSON API + a host-tested `prov_json.c` → **port it as-is**.
- `~/Documents/masterham-server/app` — an Expo **Dev Client** (not Expo Go) RN app that onboards the
  ESP32 as an AP. Borrow the `src/lib/esp32.ts` (tested) and `scripts/mock-esp32.js` patterns, but
  remove all cloud (Supabase/AWS/MQTT) and keep only local HTTP.

---

## [1] Provisioning API (SoftAP, `http://192.168.4.1`)

1:1 identical to masterham. Firmware: `components/provisioning/prov_portal.c`.

| Method/Path | Request | Response |
|---|---|---|
| `GET /api/info` | — | `{ "deviceId", "model", "apSsid" }` |
| `GET /api/scan` | — | `{ "networks":[{"ssid","rssi","secure"}] }` (cached scan) |
| `POST /api/provision` | `x-www-form-urlencoded`: `ssid`,`ssid_manual`,`password`,`tickers`,`finnhub_key`,`fmp_key`,`econ_url`,`location` (last 4 optional) | `202 {"ok":true,"state":"connecting"}` / `4xx {"ok":false,"error":<code>}` |
| `GET /api/status` | — | `{ "state":"idle\|connecting\|connected\|failed", "ssid"?, "reason"? }` |

- `provision` error codes: `ssid_empty` `ssid_too_long` `pass_too_long` `too_large` `read_error`.
  (A pathologically long password that decodes to 96 bytes or more is treated as empty by the URL
  decoder, so instead of `pass_too_long` it may be interpreted as an open network — a known limitation
  that only occurs with input far exceeding the actual WPA2 limit of 63 characters.)
- `provision` starts an asynchronous connect-test and **returns 202 immediately**. The app polls
  `/api/status` (`waitForConnected`). The connect-test validates the credentials while keeping the
  SoftAP up (`prov_wifi_connect_keep_ap`), and on success saves to NVS, reboots, and comes up in STA mode.
- ⚠️ **Single-radio limitation (important)**: the ESP32 has only one radio, so in APSTA the SoftAP
  channel follows the STA channel. As a result, right after a successful join, when the SoftAP hops to
  the target AP's channel, a phone still on channel 1 cannot reach 192.168.4.1, so it may fail to read
  `connected` and the AP may drop. Therefore, **after receiving 202, if the AP drops or status polling
  times out, the app treats it as "connected (presumed)"**, reconnects to the home Wi-Fi, and confirms
  by rediscovering the device via mDNS (`tickerboard.local`). On **failure (wrong password)**, by
  contrast, the SoftAP returns to channel 1, so the phone reads the `failed` state normally (failure
  detection is reliable; only success confirmation depends on LAN rediscovery).
- `tickers` is a comma/space-separated watchlist, the same as the existing HTML form. A single
  onboarding sets up both Wi-Fi and the watchlist.
- **API keys/URLs are entered in the app, not in menuconfig**, and stored in NVS (if empty, falls back
  to the Kconfig defaults): `finnhub_key` (stock prices), `fmp_key` (economic calendar key/proxy token),
  `econ_url` (economic calendar base URL, either FMP direct or a self-hosted proxy). These can also be
  changed after onboarding via `POST /api/stock/keys` below.
- **The captive-portal auto-popup has been removed** (DNS hijacking + OS-probe 302 redirects deleted).
  Connecting to the AP no longer brings up the "Wi-Fi login" sheet; instead, the app provisions via the
  JSON API at the fixed IP `192.168.4.1` (the browser settings page is still available as a fallback by
  visiting `http://192.168.4.1/` directly).

## [2] Stock Control API (STA, `http://tickerboard.local` or IP)

Firmware: the new `components/stock_api` (device-only httpd + mDNS) + the pure serializer
`stock_api_json.c` in `components/stock_core`. It calls the thread-safe control bridge in `user_app`.

### `GET /api/info`
Same schema as onboarding (for STA identification/discovery). `apSsid` may be an empty string.
```json
{ "deviceId":"9F3A", "model":"Ticker Board", "fw":"0.1.0", "ip":"192.168.0.42" }
```

### `GET /api/stock/state` — live snapshot (polled by the app dashboard)
```json
{
  "model":"Ticker Board", "fw":"0.1.0", "deviceId":"9F3A", "ip":"192.168.0.42",
  "index": 0,                       // currently shown watchlist index
  "page": 0,                        // 0=home 1=chart 2=news 3=metrics
  "econMode": false, "econWeek": 0, // economic-calendar overlay state
  "refreshSeconds": 30,
  "keys": { "finnhub": true, "fmp": false, "econUrl": true },   // whether each is set (values never exposed)
  "location": "Seoul",                                          // configured weather location (free text)
  "weather": { "valid": true, "tempC": 21, "city": "Seoul, KR" }, // geocoded current weather (valid=false if unresolved)
  "env": { "valid": true, "tempC": 24.3, "humidity": 41.0,
           "batteryValid": true, "batteryV": 4.02, "batteryPct": 88 },
  "watchlist": [
    { "symbol":"AAPL", "valid":true,  "price":201.5, "change":1.2, "percent":0.6, "ageSec":12 },
    { "symbol":"TSLA", "valid":false, "price":0, "change":0, "percent":0, "ageSec":-1 }
  ]
}
```
- `valid=false` / `ageSec=-1` → a slot that has never been received yet (the app shows "loading").
- Numbers are `double`. `ageSec` is the seconds elapsed since the last fetch (-1 if not yet received).

### Control (all `application/json` bodies, returning `200 {"ok":true}` immediately or `4xx {"ok":false,"error":...}`)
| Path | Body | Action |
|---|---|---|
| `POST /api/stock/select` | `{"index":2}` or `{"symbol":"TSLA"}` | Switch the on-screen ticker (same as the USER button). The symbol is resolved to an index by the firmware |
| `POST /api/stock/page` | `{"page":1}` | Switch the view (same as the BOOT button). 0..3 |
| `POST /api/stock/econ` | `{"mode":true,"week":0}` or `{"mode":false}` | Toggle the economic calendar overlay / move by week |
| `POST /api/stock/refresh` | `{"all":false}` | Force a re-fetch of the current (or all) ticker(s) |
| `POST /api/stock/watchlist` | `{"tickers":["AAPL","TSLA",...]}` or `{"tickers":"AAPL,TSLA"}` | Replace the watchlist. **Saved to NVS + applied immediately (no reboot)**. 1..16 entries; normalization is the same as `prov_tickers_parse` |
| `POST /api/stock/keys` | `{"finnhubKey"?,"fmpKey"?,"econUrl"?}` | Live-change API keys/URLs. Only present fields are updated (empty string = reset → Kconfig fallback), saved to NVS + an immediate re-fetch. Values are not returned (the `keys` in state report only whether they are configured) |
| `POST /api/stock/location` | `{"location":"Seoul"}` | Live-change the weather location. Saved to NVS + the device re-geocodes immediately (Open-Meteo, no reboot). Empty string = turn off the weather widget. The resolved city/temperature is reported via `weather` in state |

- Error codes: `bad_json` `index_range` `page_range` `symbol_not_found` `empty_watchlist` `too_many_tickers`.
  In addition, every `/api/stock/*` POST may also return the common body-reader errors `too_large`
  (body exceeded) and `read_error` (socket error), the same as provisioning `/api/provision`. For the
  watchlist token count, both the array and string forms return `too_many_tickers` when exceeding 16.
  Symbols that cannot be normalized are silently ignored (dropped).
- Write commands go into `user_app`'s command queue and are applied on the `StockTask` (LVGL/UI
  context) → the same code path as the buttons (`render_current`/`request_fetch`), so there is no race.

### `GET /api/econ` — (optional) current-week economic calendar events JSON
Used when the app mirrors the econ overlay. May return 404 if not implemented.

---

## Firmware Change Summary

- `components/provisioning`
  - New pure files **`prov_json.c/.h`** (ported from masterham) — host test (`test_prov_json.c`).
  - `prov_config.c/.h`: added the pure **`prov_validate_credentials`** (+ `prov_cred_result_t`).
  - `prov_portal.c/.h`: `/api/*` handlers + `prov_portal_info_t` + `prov_portal_set_status` +
    asynchronous provision callback (the HTML `/save` path is kept as-is).
  - `prov_wifi.c/.h`: added **`prov_wifi_connect_keep_ap`** (validation while keeping the SoftAP up).
  - `provisioning.c`: provision connect-test task + state machine.
- `components/stock_core`
  - New pure files **`stock_api_model.h`** (snapshot struct) + **`stock_api_json.c/.h`** (serializer) +
    host test.
- `components/stock_api` (new, device-only)
  - STA-mode httpd (`/api/info`,`/api/stock/*`,`/api/econ`) + mDNS (`tickerboard`).
  - Calls the `user_app` control bridge (`user_app_control.h`).
- `components/user_app`
  - `user_app_control.h`: `user_app_snapshot/select_index/set_page/set_econ/refresh/set_watchlist`.
  - `user_app.cpp`: command queue + queue set (buttons + commands), snapshot collection,
    **pre-allocates s_cache to PROV_MAX_TICKERS** (for live watchlist replacement), watchlist
    application + NVS save.
- `main/main.cpp`: starts `stock_api_start()` after STA connection.
- Dependency: added the managed component `espressif/mdns`.

## App Change Summary (`app/`)

- Expo **Dev Client** + expo-router + expo-build-properties (`usesCleartextTraffic`,
  `NSAllowsLocalNetworking`). **Expo Go not supported** → native build via `npx expo run:ios|android`.
- `src/lib/esp32.ts` — provisioning client (ported) + stock control client (extended) + types. Unit tests.
- `src/lib/discovery.ts` — base URL resolution (saved IP / `tickerboard.local` / manual input).
- Screens: onboarding (AP guide → Wi-Fi scan/select → password + watchlist → status polling) +
  control dashboard (connect → live state → watchlist editing → ticker/view/econ control → refresh).
- `scripts/mock-esp32.js` — mocks both APIs (development without hardware).

## Verification Boundaries (honestly)

- ✅ Possible: pure-logic host tests (UBSan, no ASan — hangs in the sandbox), `idf.py build`,
  app `tsc --noEmit` + `jest`, app flow via `node scripts/mock-esp32.js`.
- ⛔ Not possible (user step): actual board SPI panel / live Wi-Fi behavior, native builds on real
  iOS/Android devices.
