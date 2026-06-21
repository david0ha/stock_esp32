# Waveshare ESP32-S3-RLCD-4.2 — Hardware Specifications

ESP32-S3-based 4.2-inch reflective LCD (RLCD) development board. It supports a low-power
monochrome display, dual microphones + speaker, temperature/humidity and RTC sensors,
microSD, and battery operation.

## SoC & Memory

| Item | Value |
|------|-----|
| SoC | Espressif ESP32-S3 (Xtensa LX7 dual-core, up to 240MHz) |
| SRAM | 512KB |
| ROM | 384KB |
| PSRAM | 8MB (Octal SPI) |
| Flash | 16MB |

## Display

| Item | Value |
|------|-----|
| Type | 4.2-inch reflective LCD (RLCD), monochrome |
| Resolution | 400 × 300 |
| Controller | **ST7305 / ST7306** (Sitronix family) — naming varies by source |
| Interface | SPI (SPIM2) |
| Backlight | None (ambient-light reflective, ultra-low power) |

> A reflective LCD displays by reflecting an external light source, similar to e-paper, so it has
> no backlight and consumes very little power to hold a static screen. Its refresh rate is slower
> than a typical TFT.

## Audio

| Component | Part |
|------|------|
| Codec (output) | ES8311 low-power codec |
| Microphone ADC (input) | ES7210, dual-microphone array |
| Speaker | Onboard 8Ω 2W + external speaker header |
| Features | Noise reduction / echo cancellation support |
| Interface | I2S (data) + I2C (codec control) |

## Sensors & Timing

| Part | Function | I2C Address |
|------|------|----------|
| SHTC3 | Temperature/humidity | 0x70 |
| PCF85063A | RTC (has dedicated power header, INT=GPIO15) | 0x51 |

## Connectivity & Input

| Item | Value |
|------|-----|
| Wireless | WiFi 802.11 b/g/n, Bluetooth 5.0 LE (up to 2Mbps) |
| USB | Type-C (power/programming/charging), native USB Serial/JTAG controller |
| Storage | microSD slot (SDMMC, 1-bit bus) |
| Buttons | Power, BOOT(GPIO0), USER(GPIO18) |
| Expansion | 2× 8-pin GPIO/UART/I²C headers |
| Touch | Capacitive touch (I2C bus) |

## Digital Interfaces (ESP32-S3 general specifications)

- 45× programmable GPIO
- 4× SPI, 3× UART, 2× I2C, 2× I2S
- Full-speed USB OTG + USB Serial/JTAG
- 2× 12-bit SAR ADC (20 channels)
- 14× capacitive touch I/O
- CAN (TWAI)

## Power & Battery

| Item | Value |
|------|-----|
| Input | 5V (USB Type-C) |
| Battery | Optional 18650 lithium holder + charging circuit |
| Measurement | Battery voltage divider circuit → ADC channel 3 |
| Indicators | Charge/warning LED |

## Physical Specifications

- Dimensions: 92.5 × 70.1 × 13.5 mm (tilt 60°)
- Mounting: 4× M2.5 holes

## Development Framework Support

- **ESP-IDF** (the default choice for this repository) — [docs/esp-idf-development.md](esp-idf-development.md)
- Arduino IDE

## Notes

- A Zephyr board definition exists but is marked "Not actively maintained".
- The Waveshare official wiki may be empty as of this writing, so the pin map was cross-checked
  against the Zephyr devicetree and the specifications against Waveshare/CNX sources. See
  [docs/references.md](references.md) for the originals.
