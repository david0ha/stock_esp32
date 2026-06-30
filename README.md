# stock_esp32 — Reflective-LCD Stock & Desk Dashboard
![Uploading F4BDAF51-DB9F-4ABE-9FC6-8A1EB94FD674_1_201_a.jpeg…]()


Firmware (and a local companion app) for a always-on desk dashboard built on the
**Waveshare ESP32-S3-RLCD-4.2** board. It shows live stock quotes, weather/forecast,
and an economic calendar on a 4.2" reflective monochrome LCD (400×300, no backlight).

- **Target board:** ESP32-S3 (16 MB flash / 8 MB Octal PSRAM), ST7305/ST7306 reflective LCD
- **Framework:** ESP-IDF **v6.0.1**
- **Data sources:** Finnhub + Yahoo (quotes), Open-Meteo (weather, keyless),
  FMP **or** a bundled self-hosted proxy (economic calendar)
- **Setup:** WiFi captive portal + a local-only React Native companion app
- **Bring your own keys:** no API keys ship in this repo — you supply your own (see below)

> Detailed docs live in [`docs/`](docs/). This README is the quick start.

---

## 1. What you need

- A Waveshare ESP32-S3-RLCD-4.2 board + USB-C cable
- [ESP-IDF **v6.0.1**](https://docs.espressif.com/projects/esp-idf/en/v6.0.1/esp32s3/get-started/index.html)
- (Optional) A phone with the companion app for easy on-device control
- Free API keys for the data you want:
  - **Finnhub** (stock quotes) — free key from <https://finnhub.io>
  - **Economic calendar** — either an **FMP** key (<https://financialmodelingprep.com>, paid plan
    required for the calendar endpoint) **or** run the free bundled proxy (see §5)
  - Weather needs **no key** (Open-Meteo)

Clone with submodules (the `examples/` reference projects are git submodules):

```bash
git clone --recurse-submodules <your-fork-url> stock_esp32
cd stock_esp32
# already cloned without submodules? then:
git submodule update --init --recursive
```

---

## 2. Build & flash

Activate the ESP-IDF environment **once per shell session**:

```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"   # adjust to your IDF install path
```

Then, from the repo root (the repo root **is** the ESP-IDF project):

```bash
idf.py set-target esp32s3            # once per checkout
idf.py menuconfig                    # Flash 16MB / PSRAM 8MB (Octal); optional key defaults (§5)
idf.py build
idf.py -p <PORT> flash monitor       # Ctrl+] to exit the monitor
```

- `<PORT>` on macOS is usually `/dev/cu.usbmodem*` (native USB Serial/JTAG) or
  `/dev/cu.usbserial-*` (UART bridge). Run `ls /dev/cu.*` to find it.
- If the board won't enter flash mode: hold **BOOT**, tap **RESET**, release BOOT, retry.

You do **not** have to put API keys in the firmware build — the easiest path is to enter
them at runtime during setup (§3) or later from the companion app (§4).

---

## 3. First-time setup (AP / Wi-Fi provisioning mode)

On first boot (no saved Wi-Fi) the device starts a **captive-portal access point**:

- **SoftAP SSID:** `Ticker Board-XXXX` (`XXXX` = last 4 hex of the MAC)
- **Security:** open network (no AP password)
- **Portal address:** `http://192.168.4.1/`

You can provision in two ways:

**A) Companion app (recommended — also sets API keys).** Join the `Ticker Board-XXXX`
network on your phone, open the app, and it talks to the device at `192.168.4.1`. The app
collects: Wi-Fi SSID + password, watchlist tickers, weather location, and your
**Finnhub / FMP keys + economic-calendar URL**.

**B) Browser portal.** Join `Ticker Board-XXXX`, open `http://192.168.4.1/` in a browser.
The web form collects **Wi-Fi SSID + password, watchlist tickers, and weather location**.
(The browser form does *not* take API keys — use the app or `menuconfig` for those.)

After you submit, the device reboots, joins your home Wi-Fi (STA mode), and starts the
dashboard. It then advertises itself over mDNS as **`tickerboard.local`**.

---

## 4. Normal operation & companion app

Once on your Wi-Fi, the local-only React Native app (in [`app/`](app/), see
[docs/app-control.md](docs/app-control.md)) controls the device over HTTP/JSON, discovering
it at **`http://tickerboard.local`** (or its IP). From the app you can:

- Edit the watchlist, switch the selected symbol, change pages
- Set the weather location
- Update data-source keys live (no reboot): `POST /api/stock/keys`
  with `{ "finnhubKey": "...", "fmpKey": "...", "econUrl": "..." }`
- Open the economic-calendar view and page through weeks

The device speaks plain HTTP on the LAN only (no cloud). See
[docs/stock-app.md](docs/stock-app.md) for the full firmware behavior.

---

## 5. Economic calendar

The calendar can be powered two ways. Pick one and set the **base URL + key**.

### Option A — FMP (paid)
Use Financial Modeling Prep directly. The calendar endpoint requires a **paid** FMP plan
(free keys return HTTP 402).

- Base URL (default): `https://financialmodelingprep.com/stable/economic-calendar`
- Key: your FMP API key

### Option B — self-hosted free proxy (recommended if you don't pay FMP)
This repo bundles a small proxy in [`tools/econ_proxy/`](tools/econ_proxy/) that serves the
same shape by scraping investing.com (free). Deploy it on a Mac mini / Linux box / VM and
point the device at it. Full guide: [docs/econ-proxy-deployment.md](docs/econ-proxy-deployment.md).

- Base URL: `http://<proxy-host>:8442/economic-calendar` (or your `https://` tunnel hostname)
- Key (`fmpKey`): the proxy ignores the key value, but the firmware requires a **non-empty**
  string — put any placeholder, **or** the proxy's shared-secret token if you enabled auth
- ⚠️ If you expose the proxy publicly, set `ECON_PROXY_TOKEN` and use it as the key

### Where to set it

| Where | How |
|-------|-----|
| Build time | `idf.py menuconfig` → `STOCK_ECON_BASE_URL`, `STOCK_FMP_API_KEY` |
| Onboarding (app) | app sends `econ_url` + `fmp_key` to `/api/provision` |
| Runtime (app) | `POST /api/stock/keys { "fmpKey": "...", "econUrl": "..." }` |

Relevant build options ([`components/stock_core/Kconfig.projbuild`](components/stock_core/Kconfig.projbuild)):

- `STOCK_ECON_BASE_URL` — default is the FMP endpoint above
- `STOCK_FMP_API_KEY` — empty by default (you must supply one, or use the proxy)
- `STOCK_ECON_MIN_IMPACT` — minimum event importance shown: `1` = Low+Med+High,
  `2` = Med+High (default), `3` = High only (the 400×300 panel fits ~14 rows/week)

### Open the calendar on the device
Press **USER + BOOT together and release quickly** (a short chord) to toggle the
economic-calendar view. While it's open, **USER = previous week**, **BOOT = next week**.
If no key/proxy is configured you'll see an error on screen instead.

---

## 6. Returning to AP (Wi-Fi setup) mode

To re-run provisioning after the device is already on your Wi-Fi (e.g. you moved, changed
the router, or want to re-enter keys):

> **Hold USER + BOOT together for ~5 seconds.**

The screen shows *"Wi-Fi setup mode — restarting…"*, the device reboots back into the
`Ticker Board-XXXX` access point, and the portal **pre-fills your saved settings** so you
only change what you need. (Short press of the same two buttons = open the calendar; the
5-second hold is what triggers setup mode.)

There is no remote/API way to force setup mode — the button hold is intentional, so a LAN
peer can't kick your device offline.

### Button reference
| Button | GPIO | Action |
|--------|------|--------|
| USER | 18 | short chord (with BOOT): toggle calendar / prev week |
| BOOT | 0 | short chord (with USER): toggle calendar / next week |
| USER + BOOT | — | **hold 5 s → Wi-Fi setup (AP) mode** |
| RESET / Power | — | hardware reset / power |

---

## 7. Repository layout

| Path | What |
|------|------|
| `components/stock_core` | portable core + device port (display, data fetch seams) |
| `components/user_app` | app glue, input handling, rendering |
| `components/provisioning` | Wi-Fi STA/SoftAP, NVS, SNTP, captive portal, `/api/*` |
| `components/stock_api` | STA-mode HTTP/JSON control server + mDNS (`tickerboard.local`) |
| `app/` | local-only React Native companion app (Expo Dev Client) |
| `sim/` | desktop LVGL UI simulator (no board needed) |
| `tools/econ_proxy/` | free economic-calendar proxy (self-hosted) |
| `third_party/` | vendored cJSON (MIT) |
| `docs/` | hardware, pinout, build, graphics, weather, app-control, references |
| `examples/` | reference projects (git submodules) |

More: [docs/board-hardware.md](docs/board-hardware.md) · [docs/pinout.md](docs/pinout.md) ·
[docs/esp-idf-development.md](docs/esp-idf-development.md) · [docs/simulator.md](docs/simulator.md)

---

## 8. Security note (for self-hosters)

The STA-mode control API (`/api/stock/*`) is currently **unauthenticated** and intended for
trusted home LANs only. Do not expose the device directly to the internet. If you publicly
expose the econ proxy, always set `ECON_PROXY_TOKEN`. Keep your real API keys out of git —
they live in your local `.env` / `sdkconfig`, which are gitignored.

---

## License

[MIT](LICENSE). Vendored cJSON is also MIT (see `third_party/cJSON`).
