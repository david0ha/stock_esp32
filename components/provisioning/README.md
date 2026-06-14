# provisioning

Wi-Fi + stock-ticker provisioning for the ESP32-S3 Ticker Board, with a captive-portal
fallback. On boot the device joins the saved network; if that fails (or nothing is saved
yet) it raises a SoftAP and serves a setup page where the user enters Wi-Fi credentials and
a watchlist of tickers. The submission is stored in NVS and the device reboots and connects.

## Boot flow

```
load config (NVS)
      │
有 saved SSID ──yes──▶ try STA connect (timeout) ──ok──▶ return connected → app runs
      │                          │
      no                        fail
      ▼                          ▼
  start SoftAP "Ticker Board-XXXX" (open) + captive portal
      │
  user submits SSID / password / tickers  →  save to NVS  →  reboot
```

The auto-fallback is simply the loop closing on itself: a bad password means the next boot's
STA attempt fails and the portal comes back up.

## Layers

**Pure logic (host-unit-tested, no ESP-IDF dependency):**

| File | Responsibility |
|------|----------------|
| `prov_config.{h,c}` | config model; ticker normalize / parse / serialize |
| `form_parse.{h,c}`  | `x-www-form-urlencoded` decode + field extraction |
| `json_build.{h,c}`  | JSON string escaping for `/scan` and `/state` |

**Embedded glue (verified by build + on-device):**

| File | Responsibility |
|------|----------------|
| `prov_store.{h,c}`  | NVS load/save/clear (namespace `prov`); `prov_store_save` returns commit status |
| `prov_wifi.{h,c}`   | STA connect (bounded initial retry → then **persistent** reconnect once online), SoftAP, scan |
| `prov_portal.{h,c}` | HTTP server + DNS hijack captive portal (rejects over-length / NUL-injected fields) |
| `provisioning.{h,c}`| orchestrator (`provisioning_run`) + public API; only reboots on a confirmed save |
| `net_time.{h,c}`    | one-shot SNTP sync after connect (news window + chart labels); deinits when done |
| `portal.html`       | self-contained setup page (embedded via `EMBED_TXTFILES`) |

## HTTP endpoints (SoftAP, 192.168.4.1)

| Method | Path     | Purpose |
|--------|----------|---------|
| GET    | `/`      | setup page |
| GET    | `/scan`  | `{"networks":[{"ssid","rssi","secure"}, …]}` |
| GET    | `/state` | `{"ssid","tickers":[…]}` to pre-fill the form |
| POST   | `/save`  | body `ssid=…&password=…&tickers=AAPL,TSLA` → `{"ok":true}` then reboot |
| *      | *(other)*| 302 → `http://192.168.4.1/` (OS captive-portal detection) |

A UDP DNS responder on port 53 answers every A query with `192.168.4.1` so phones pop the
captive sheet automatically.

## NVS keys (namespace `prov`)

`ssid` (str) · `pass` (str) · `tickers` (str, comma-separated, e.g. `AAPL,TSLA,MSFT`).

## Host tests

The pure logic has a self-contained test harness (no external framework):

```sh
./test/run.sh
```

Compiles `prov_config.c` / `form_parse.c` / `json_build.c` with the tests under `test/`
using UndefinedBehaviorSanitizer and runs them. (AddressSanitizer is intentionally omitted —
its shadow-memory mmap is blocked in the CI sandbox.)
