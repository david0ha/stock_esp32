# ESP32-S3-RLCD-4.2 — Pin Assignment Table

The GPIO numbers below were extracted from the `pinctrl` and board dts of the Zephyr board
devicetree (`esp32s3_rlcd_4_2`). In ESP-IDF you can use the same physical pins as-is.

> Zephyr notation conversion: `gpio0.N` = GPIO N, `gpio1.N` = GPIO (32+N).
> e.g., CS `gpio1.8` = GPIO40, RST `gpio1.9` = GPIO41, DC `gpio0.5` = GPIO5.

## Display — SPI (SPIM2, ST7305/ST7306)

| Signal | GPIO |
|------|------|
| SCLK (Clock) | 11 |
| MOSI | 12 |
| CS (Chip Select, active-low) | 40 |
| DC (Data/Command, active-high) | 5 |
| RESET (active-low) | 41 |

> Reflective monochrome panel, so there is no backlight pin.

## I2C0 (Touch / RTC / Temperature-Humidity)

| Signal | GPIO |
|------|------|
| SDA | 13 |
| SCL | 14 |

| Device | Address | Notes |
|----------|------|------|
| PCF85063A RTC | 0x51 | INT1 = GPIO15 |
| SHTC3 temperature/humidity | 0x70 | |
| Capacitive touch controller | (check board documentation) | Same I2C bus |

## Audio — I2S0 (ES8311 / ES7210)

| Signal | GPIO |
|------|------|
| MCLK | 16 |
| BCLK (Bit Clock, out) | 9 |
| WS (Word Select, out) | 45 |
| DOUT (Serial Data out) | 10 |
| DIN (Serial Data in) | 8 |

> The codec control registers are accessed via I2C0 (GPIO13/14).

## microSD — SDMMC (SDHC0, 1-bit)

| Signal | GPIO |
|------|------|
| CMD | 21 |
| CLK | 38 |
| D0 | 39 |

## UART0 (Console)

| Signal | GPIO |
|------|------|
| TX | 43 |
| RX | 44 |

> Default baud rate 115200. Console output is also available via the ESP32-S3 native USB Serial/JTAG.

## Buttons

| Button | GPIO | Notes |
|------|------|------|
| BOOT (Button0) | 0 | pull-up, active-low |
| USER (Button1) | 18 | pull-up, active-low |
| Power | (power circuit) | |

## ADC

| Purpose | Channel | Notes |
|------|------|------|
| Battery voltage (vbatt) | ADC channel 3 | Via voltage divider circuit |

## Items Requiring Confirmation

- Touch controller I2C address/INT pin: must be confirmed via the Waveshare schematic/examples.
- RGB/status LED pin: sources conflict (some mention GPIO38, but GPIO38 is used as
  SDMMC CLK), so it must be confirmed via the board schematic.
- Always re-verify the exact pins against the original Zephyr devicetree in [docs/references.md](references.md).
