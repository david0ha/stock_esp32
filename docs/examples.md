# Reference Examples Collection (examples/)

External examples for the ESP32-S3-RLCD-4.2 board are registered under `examples/` as **git submodules**.
(They retain the original repositories' history as-is, and are managed so there are no conflicts/warnings when committing to this repository.)

## How to use submodules

```bash
# Fetch the contents on first clone / when empty
git submodule update --init --recursive

# Update all examples to the latest
git submodule update --remote
```

Each example is an independent repository, so you can run `git pull` and the like separately inside its folder.
From this repository's perspective, it only tracks "a pointer to a specific commit," so it is safe.

---

## 1. `examples/waveshare-official` — Waveshare official demos ⭐

- Source: https://github.com/waveshareteam/ESP32-S3-RLCD-4.2
- The most authoritative primary material. Official examples/libraries/firmware from the board manufacturer.
- Structure:
  - `01_Arduino_Libraries/` — SensorLib, U8g2, LVGL8, LVGL9 (dependency libraries)
  - `02_Example/` — 4 kinds of examples: **Arduino / ESP-IDF / ESPHome / XiaoZhi**
  - `03_Firmware/` — pre-built firmware (`01_Factory_V1.bin`, `02_XiaoZhi_V2.1.0.bin`)
  - `Tools-Configuration.png` — Arduino IDE board settings reference
- **When developing with ESP-IDF, refer to `02_Example/ESP-IDF` first.** You can find the official
  implementations of the display/audio/sensor drivers here.

## 2. `examples/VolosR-waveshareLRCL` — Volos clock/weather demo

- Source: https://github.com/VolosR/waveshareLRCL
- Video: https://www.youtube.com/watch?v=MziW1InJwa4
- **Arduino** (`espRLCD/espRLCD.ino`). Drives the ST7305/ST7306 panel over SPI with a custom
  `display_bsp` (DisplayPort class), and renders with `Adafruit_GFX` + `GFXcanvas1` (1bpp).
- Very useful for confirming the pin map: `DisplayPort(12,11,5,40,41,W,H)`
  = **MOSI=12, SCLK=11, DC=5, CS=40, RST=41**, battery ADC=GPIO4, SHTC3=0x70, PCF85063A RTC.
- A clock/temperature-humidity/battery display example that emphasizes sunlight readability.

## 3. `examples/VolosR-eBikeRLCD` — Volos E-Bike dashboard

- Source: https://github.com/VolosR/eBikeRLCD
- Video: https://www.youtube.com/watch?v=EKnZ7ZisUj4
- **Arduino** (`espRLCD/espRLCD.ino`). Based on the same `display_bsp` as waveshareLRCL.
- For reference on a dashboard UI pattern that displays speed/ride data legibly even in sunlight
  using large fonts (`bigFont`/`midleFont`/`smallFont`/`valueFont`).

## 4. `examples/JasonHEngineering-RLCD-clock` — Smart clock (includes audio/RTC)

- Source: https://github.com/JasonHEngineering/waveshare_RLCD_400x300_monochrome
- Video: https://www.youtube.com/watch?v=kfiwg6aNjsw
- Blog: https://jashuang1983.wordpress.com/waveshare-4-2-rlcd-customized-monochrome-smart-clock/
- **Arduino**. Two versions, V1/V2, + 3D-print STEP files + a Python script to generate monochrome images.
- Included drivers: `display_bsp`, **`codec_bsp` (ES8311/ES7210 audio)**,
  **`PCF85063A` (RTC)** — especially useful as an audio/RTC integration reference.
- Features: internet time sync, bus arrival times, trivia, ChatGPT queries via audio commands
  (some features are localized to Singapore).

---

## Material with only video/blog (no code repository)

Material worth referencing that was not registered as a submodule:

- **Draeger-IT (Part 1)** — unboxing/first impressions ("ePaper killer?")
  - Video: https://www.youtube.com/watch?v=TG7722V8UGw
  - Blog: https://draeger-it.blog/esp32-s3-4-2-display-epaper-killer-unboxing/
- **Draeger-IT (Part 2)** — how to run Arduino IDE + **LVGL**
  - Video: https://www.youtube.com/watch?v=-ohhYlIscEo
  - Blog: https://draeger-it.blog/esp32-s3-rlcd-display-ansteuern-so-klappt-die-programmierung-mit-arduino-ide-teil-2/

---

## Note: framework memo

- All 4 external examples above are **Arduino**-based. This repository's own development uses **ESP-IDF**
  (→ [esp-idf-development.md](esp-idf-development.md)), so refer to the examples' **pin maps, driver
  logic, and UI patterns**, but port the build system over to ESP-IDF.
- If you need a native ESP-IDF implementation, look at `examples/waveshare-official/02_Example/ESP-IDF` first.
