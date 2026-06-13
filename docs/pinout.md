# ESP32-S3-RLCD-4.2 — 핀 할당표

아래 GPIO 번호는 Zephyr 보드 devicetree(`esp32s3_rlcd_4_2`)의 `pinctrl` 및
보드 dts에서 추출한 것이다. ESP-IDF에서도 동일한 물리 핀을 그대로 사용하면 된다.

> Zephyr 표기 변환: `gpio0.N` = GPIO N, `gpio1.N` = GPIO (32+N).
> 예) CS `gpio1.8` = GPIO40, RST `gpio1.9` = GPIO41, DC `gpio0.5` = GPIO5.

## 디스플레이 — SPI (SPIM2, ST7305/ST7306)

| 신호 | GPIO |
|------|------|
| SCLK (Clock) | 11 |
| MOSI | 12 |
| CS (Chip Select, active-low) | 40 |
| DC (Data/Command, active-high) | 5 |
| RESET (active-low) | 41 |

> 반사형 흑백 패널이므로 백라이트 핀 없음.

## I2C0 (터치 / RTC / 온습도)

| 신호 | GPIO |
|------|------|
| SDA | 13 |
| SCL | 14 |

| 디바이스 | 주소 | 비고 |
|----------|------|------|
| PCF85063A RTC | 0x51 | INT1 = GPIO15 |
| SHTC3 온습도 | 0x70 | |
| 정전식 터치 컨트롤러 | (보드 자료 확인) | 동일 I2C 버스 |

## 오디오 — I2S0 (ES8311 / ES7210)

| 신호 | GPIO |
|------|------|
| MCLK | 16 |
| BCLK (Bit Clock, out) | 9 |
| WS (Word Select, out) | 45 |
| DOUT (Serial Data out) | 10 |
| DIN (Serial Data in) | 8 |

> 코덱 제어 레지스터는 I2C0(GPIO13/14)를 통해 접근.

## microSD — SDMMC (SDHC0, 1-bit)

| 신호 | GPIO |
|------|------|
| CMD | 21 |
| CLK | 38 |
| D0 | 39 |

## UART0 (콘솔)

| 신호 | GPIO |
|------|------|
| TX | 43 |
| RX | 44 |

> 기본 보레이트 115200. ESP32-S3 네이티브 USB Serial/JTAG로도 콘솔 출력 가능.

## 버튼

| 버튼 | GPIO | 비고 |
|------|------|------|
| BOOT (Button0) | 0 | pull-up, active-low |
| USER (Button1) | 18 | pull-up, active-low |
| Power | (전원 회로) | |

## ADC

| 용도 | 채널 | 비고 |
|------|------|------|
| 배터리 전압 (vbatt) | ADC 채널 3 | 분압 회로 경유 |

## 확인이 필요한 항목

- 터치 컨트롤러 I2C 주소/INT 핀: Waveshare 회로도/예제로 확정 필요.
- RGB/상태 LED 핀: 자료 간 상충(일부 자료 GPIO38 언급, 그러나 GPIO38은
  SDMMC CLK로 사용됨)이 있어 보드 회로도로 확정 필요.
- 정확한 핀은 항상 [docs/references.md](references.md)의 Zephyr devicetree 원본으로 재확인.
