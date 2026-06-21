# Stock Monitor App

An ESP32 stock price monitor for individual investors. It consists of a fixed top bar (ticker / current price / change %) and a bottom area that cycles every 3 seconds (1. intraday minute-bar chart, 2. news headlines, 3. investment metrics).
It is rendered with LVGL v9 and designed with pure black-and-white and large fonts to suit the 400×300 reflective monochrome panel.

## Architecture — Platform Dependency in One Place Only

```
components/stock_core/                 (mostly platform-independent, shared by firmware/simulator/tests)
├── include/stock_model.h    data model (quote / series / metrics / news)
├── stock_parse.c            JSON → model parser (pure functions, unit-test target)
├── stock_service.c          URL assembly + http_get calls + parsing orchestration
├── include/http_port.h      ★ the only platform boundary: char *http_get(url, *status)
├── ui_stock.c               LVGL UI (top bar + 3 pages + page dots)
├── http_port_esp.c          [firmware-only] esp_http_client + TLS certificate bundle (PSRAM accumulation)
└── Kconfig.projbuild        user settings (below)

components/provisioning/               [firmware-only] WiFi connection + ticker provisioning
├── prov_config.c            ticker normalization/parsing/serialization + watchlist rotation (pure, unit-test target)
├── form_parse.c·json_build.c  form decode / JSON escaping (pure, unit-test target)
├── prov_wifi.c              STA (retry+timeout, persistent reconnect after connecting) + SoftAP + scan
├── prov_portal.c            captive portal (HTTP + DNS hijacking) · portal.html
├── prov_store.c             NVS save/load (namespace prov)
├── provisioning.c           orchestrator: connect to saved network → portal on failure → save → reboot
└── net_time.c               one-time SNTP time sync after connecting (for news periods/chart labels)

components/user_app/user_app.cpp  app glue: cJSON PSRAM hooks + watchlist rotation fetch task
main/main.cpp                     boot: status screen → provisioning_run → net_time_sync → stock UI

sim/http_port_curl.c     [simulator-only] same http_get implemented with libcurl
```

> **Boot flow**: `app_main` brings up NVS/display/LVGL, shows the setup guidance screen, then
> calls `provisioning_run()`. If saved WiFi exists, it connects (falling back to the captive portal on failure);
> once connected, it sets the time with `net_time_sync()` and brings up the stock UI. After that, the fetch task
> switches to the next ticker in the watchlist every `STOCK_REFRESH_SECONDS` and refreshes the data.

All code above `http_get` (parser, service, UI) is compiled **literally identically** by the firmware and the simulator.
This makes it possible to verify the full fetch→parse→render path with real data on a Mac (with internet access)
via screenshots.

## Data Sources

| Screen Element | Source | Endpoint |
|-----------|------|-----------|
| current price / change / change % | **Finnhub** `/quote` | key required, good real-time quality |
| investment metrics (PER·EPS·market cap·52-week·dividend) | **Finnhub** `/stock/metric?metric=all` | key required (~240KB → PSRAM) |
| news headlines (last 7 days) | **Finnhub** `/company-news` | key required |
| intraday minute-bar line chart | **Yahoo** v8 `/chart?range=1d&interval=5m` | no key required |

> The Finnhub free plan blocks `/stock/candle` (minute bars), so only the minute-bar chart uses Yahoo.
> Yahoo v8 chart requires no key but has per-IP rate limits (see limitations below).

## Refresh Intervals

- **Screen cycling**: cycles chart→news→metrics every `CONFIG_STOCK_ROTATE_SECONDS` (default 3 seconds).
- **Data re-request**: background fetch every `CONFIG_STOCK_REFRESH_SECONDS` (default 30 seconds).
  The screen draws from cache, so the 3-second cycle does not re-fetch each time (rate-limit protection).

## Configuration

### Runtime Provisioning (the Board's Captive Portal)

WiFi credentials and the **watchlist (ticker list)** are entered and saved to NVS via the captive portal
(`http://192.168.4.1`) of the open SoftAP `Ticker Board-XXXX` that the board brings up at boot.
The user configures them directly without modifying code/Kconfig; if the password is wrong, the next boot connection fails
and the portal automatically reappears. Once the connection succeeds, the app cycles through the entered watchlist.

### Kconfig (menuconfig → "Stock Monitor")

| Kconfig | Default | Description |
|---------|--------|------|
| `STOCK_FINNHUB_API_KEY` | (empty) | Finnhub free key. **Do not commit a real key** |
| `STOCK_SYMBOL` | `AAPL` | fallback ticker used only when the watchlist is empty |
| `STOCK_ROTATE_SECONDS` | 3 | screen (chart/news/metrics) cycling interval |
| `STOCK_REFRESH_SECONDS` | 30 | data re-request interval (switches to the next watchlist ticker at this point) |

The Finnhub key is stored only in Kconfig (local `sdkconfig`, gitignored). **The WiFi password is no longer stored
in Kconfig but only via portal→NVS** (no credentials are committed to the repository).

## Build / Run

### Simulator (no board, verify real data on a Mac)
```bash
cd sim
FINNHUB_KEY=<key> STOCK_SYMBOL=AAPL ./sim.sh
# → shots/sim_page0.png(chart) sim_page1.png(news) sim_page2.png(metrics)
```

### Host Unit Tests (parser)
```bash
cd components/stock_core/test/host
cmake -S . -B build && cmake --build build && ./build/test_parse
```

### Host Unit Tests (provisioning pure logic)
```bash
sh components/provisioning/test/run.sh   # ticker normalization/rotation · form decode · JSON escaping
```

### Firmware (device)
```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"
cd <repo root>            # ESP-IDF project = repository root
idf.py menuconfig          # set key/SSID/symbol in the Stock Monitor menu
idf.py build
idf.py -p <PORT> flash monitor
```

## Verification Status

- ✅ Parser: passes host unit tests (real Finnhub responses + Yahoo schema fixtures).
- ✅ Data + UI: 3-page rendering screenshots verified with **real data** in the simulator.
- ✅ Firmware: `idf.py build` compiles successfully.
- ⛔ Real device operation (SPI panel/WiFi/actual measured contrast) can only be verified **on the board** — a user step.

## Things to Know / Limitations

- **Yahoo rate limit**: if the development host's (Mac) IP is blocked, the simulator substitutes only the chart
  with synthetic data to verify the renderer (marked `SYNTHETIC` in the log). The board has a different IP, so it usually works normally.
  On persistent 429s, increase the polling interval or rely on the cache.
- **Monochrome fonts**: characters not present in the built-in Montserrat (curly quotes·dashes·…) are replaced with ASCII
  in the parser to prevent tofu (□) (`to_ascii`). Non-ASCII symbols/news may be displayed in simplified form.
- **Memory**: Finnhub `metric=all` (~240KB) is accumulated in PSRAM, and the cJSON tree is also placed in PSRAM
  (`cJSON_InitHooks`). There is plenty of room on 8MB PSRAM.
- **WiFi/provisioning**: `components/provisioning` handles STA connection·SoftAP·captive portal·NVS saving
  all together (including `net_time`'s SNTP sync). After the first successful connection, it reconnects indefinitely even if dropped.
  The network scan on the setup screen may briefly disconnect the connected phone due to the single-radio nature
  (recover with a manual ⟳ rescan or by directly entering "Other network…").
- **TLS certificate (important)**: finnhub.io cross-signs the Google Trust Services chain with the **original "GlobalSign Root CA" (1998)**.
  The ESP-IDF full bundle has only GlobalSign R3/R6·GTS R1~R4 and not this original root, so
  every finnhub call fails with `No matching trusted root certificate found` (Yahoo=DigiCert works fine).
  Fix: `certs/globalsign_root_ca.pem` was added to the full bundle via `CONFIG_MBEDTLS_CUSTOM_CERTIFICATE_BUNDLE` (+PATH)
  (reflected in sdkconfig.defaults). This root expires 2028-01 — finnhub is likely to renew its chain before then,
  at which point the bundle's GTS R4 will be sufficient.
```
