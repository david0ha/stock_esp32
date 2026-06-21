# References — ESP32-S3-RLCD-4.2

Original source links for information about this board. When in doubt about specifications/pin maps, re-verify here.

## Board Documentation

- **Zephyr board documentation** (specifications + programming/debugging overview)
  https://docs.zephyrproject.org/latest/boards/waveshare/esp32s3_rlcd_4_2/doc/index.html
- **Zephyr board source** (devicetree·pinctrl = primary source for the pin map)
  https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/waveshare/esp32s3_rlcd_4_2
  - `esp32s3_rlcd_4_2-pinctrl.dtsi` — UART/SPI/I2C/I2S/SDMMC pins
  - `esp32s3_rlcd_4_2_esp32s3_procpu.dts` — devices/addresses/buttons/ADC
- **Waveshare official documentation** (may be empty as of this writing)
  https://docs.waveshare.com/ESP32-S3-RLCD-4.2
- **CNX Software introduction article** (specifications·audio configuration)
  https://www.cnx-software.com/2026/01/06/esp32-s3-development-board-features-4-2-inch-reflective-lcd-rlcd-dual-microphone-array-onboard-speaker/

## ESP-IDF

- ESP-IDF Programming Guide (ESP32-S3)
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/
- esp_lcd (display abstraction)
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/lcd/index.html
- ESP Component Registry (driver search: ST7305/ST7306, ES8311, ES7210, etc.)
  https://components.espressif.com/
- esp_codec_dev (audio codec driver)
  https://components.espressif.com/components/espressif/esp_codec_dev

## Chip/Component Datasheets

- ESP32-S3 datasheet: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- ST7305 / ST7306 (Sitronix reflective monochrome LCD controller) — search the manufacturer datasheet
- ES8311 (audio codec), ES7210 (microphone ADC) — Everest Semi datasheets
- SHTC3 (Sensirion temperature/humidity), PCF85063A (NXP RTC) — manufacturer datasheets

## Local Environment

- ESP-IDF v6.0.1 install path: `~/.espressif/v6.0.1/`
- Activation script: `source "~/.espressif/tools/activate_idf_v6.0.1.sh"`
