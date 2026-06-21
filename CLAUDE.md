# CLAUDE.md

The new development target for this repository (`stock_esp32`) is the **Waveshare ESP32-S3-RLCD-4.2** board,
developed with the **ESP-IDF v6.0.1** framework. This document summarizes the core board/toolchain
information so you can work without having to research it again. For details, see `docs/`.

## Quick start (do this first)

Activate the ESP-IDF environment — you must **run this once per new shell session** to use `idf.py`:

```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"
```

Standard workflow after activation:

```bash
idf.py set-target esp32s3      # Specify the target chip (once per project)
idf.py menuconfig              # Configure Flash 16MB / PSRAM 8MB (Octal)
idf.py build                   # Build
idf.py -p <PORT> flash monitor # Flash + serial monitor (exit with Ctrl+])
```

- `<PORT>`: when the board is connected on macOS, usually `/dev/cu.usbmodem*` (USB Serial/JTAG) or
  `/dev/cu.usbserial-*` (UART bridge). Check with `ls /dev/cu.*`.
- If it won't enter flash mode, **hold the BOOT button while pressing RESET**, release, and try again.

> **Project structure**: the root of this repository is itself the ESP-IDF project — `CMakeLists.txt`, `main/`, and `components/`
> are at the root, so run `idf.py` **from the repository root**. The firmware app consists of `components/stock_core`
> (portable core + device port) + `components/user_app` (app glue); `components/provisioning`, which configures
> WiFi/tickers via a captive portal (includes WiFi STA, SoftAP, NVS, SNTP `net_time`, plus the
> `/api/*` JSON provisioning API for the companion app); `components/stock_api`, which brings up an HTTP/JSON +
> mDNS (`tickerboard.local`) server so the app can control the stock features in STA mode; the local-only React Native companion
> app in `app/`; the desktop simulator for checking the UI without the board in `sim/`; and the vendored cJSON in
> `third_party/`. For details see [docs/stock-app.md](docs/stock-app.md) ·
> [docs/app-control.md](docs/app-control.md).

## Target board at a glance

| Item | Specification |
|------|------|
| SoC | ESP32-S3 (Xtensa LX7 dual-core, up to 240MHz) |
| Flash / PSRAM | 16MB Flash / 8MB PSRAM (Octal) |
| Display | 4.2" reflective monochrome LCD, 400×300, **ST7305/ST7306** controller, SPI connection, no backlight |
| Touch | Capacitive touch (I2C) |
| Audio | ES8311 codec + ES7210 dual-mic ADC + 8Ω 2W speaker (I2S) |
| Sensor/RTC | SHTC3 temperature/humidity (0x70), PCF85063A RTC (0x51) |
| Storage | microSD (SDMMC, 1-bit) |
| Wireless | WiFi 802.11 b/g/n, BLE 5.0 |
| USB | Type-C (power/programming/charging), native USB Serial/JTAG |
| Buttons | Power, BOOT(GPIO0), USER(GPIO18) |
| Power | 5V USB-C, optional 18650 battery + charging circuit, battery voltage ADC measurement |

> Controller naming: the Zephyr devicetree denotes it as `ST7306`, while Waveshare/CNX materials denote it as `ST7305`.
> Both are Sitronix-family reflective monochrome controllers; check both when writing a driver.

Core pinout (summary): SPI display `SCLK=11, MOSI=12, CS=40, DC=5, RST=41` ·
I2C `SDA=13, SCL=14` · I2S `MCLK=16, BCLK=9, WS=45, DOUT=10, DIN=8` ·
SD `CMD=21, CLK=38, D0=39` · UART0 `TX=43, RX=44`. Full table at [docs/pinout.md](docs/pinout.md).

## Documentation structure (docs/)

- [docs/board-hardware.md](docs/board-hardware.md) — full board hardware specifications
- [docs/pinout.md](docs/pinout.md) — GPIO pin assignment table per peripheral
- [docs/esp-idf-development.md](docs/esp-idf-development.md) — ESP-IDF install/build/flash/menuconfig/driver guide
- [docs/examples.md](docs/examples.md) — description of the reference examples (submodules) registered under `examples/`
- [docs/graphics.md](docs/graphics.md) — 1-bit graphics rendering approaches (immediate-mode GFX vs LVGL) and selection guide
- [docs/simulator.md](docs/simulator.md) — desktop simulator (`sim/`) for checking the LVGL UI without the board
- [docs/weather.md](docs/weather.md) — home weather/forecast widget (free, no-key Open-Meteo) + portal location-name input and geocoding flow
- [docs/econ-proxy-deployment.md](docs/econ-proxy-deployment.md) — economic calendar proxy (`tools/econ_proxy`, port 8442) deployment/review guide (Mac mini/Linux VM)
- [docs/app-control.md](docs/app-control.md) — local-only RN companion app (`app/`) ↔ firmware HTTP/JSON control contract (provisioning `/api/*` + stock control `/api/stock/*` + mDNS)
- [docs/references.md](docs/references.md) — official documentation/datasheet links

## Reference examples (examples/, git submodule)

External examples for this board are managed as submodules. To fetch their contents on first clone:

```bash
git submodule update --init --recursive
```

- `examples/waveshare-official` — Waveshare official demos (Arduino/ESP-IDF/ESPHome/XiaoZhi) ⭐
- `examples/VolosR-waveshareLRCL` — Volos clock/weather (Arduino)
- `examples/VolosR-eBikeRLCD` — Volos E-Bike dashboard (Arduino)
- `examples/JasonHEngineering-RLCD-clock` — smart clock, includes audio/RTC (Arduino)

Details and video/blog links are in [docs/examples.md](docs/examples.md). Since the examples are Arduino-based,
use them as a reference for pinout, drivers, and UI patterns, but port the build to ESP-IDF.

## Existing projects in the repository (reference / different boards)

The following 3 are previous work and use **hardware different from the target board (ESP32-S3-RLCD-4.2)**. For pattern reference.

- `ESP32_Stock_Ticker/` — Arduino-based multi-symbol ticker (Finnhub API, custom PCB)
- `espstock/` — ESPHome 8-segment stock price display
- `finance-monitor/` — PlatformIO/Arduino, ESP32-2432S028R (Cheap Yellow Display, ILI9341), Yahoo Finance + RSS

## Working rules

- Before any ESP-IDF command, the `source ...activate_idf_v6.0.1.sh` above must always be run first.
- When adding display/audio/sensor drivers, first check the GPIO in [docs/pinout.md](docs/pinout.md) and
  the component guide in [docs/esp-idf-development.md](docs/esp-idf-development.md).
- If anything about the board specifications is uncertain, do not guess — check the original sources in [docs/references.md](docs/references.md).
