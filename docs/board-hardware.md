# Waveshare ESP32-S3-RLCD-4.2 — 하드웨어 사양

ESP32-S3 기반 4.2인치 반사형 LCD(RLCD) 개발 보드. 저전력 흑백 디스플레이,
듀얼 마이크 + 스피커, 온습도/RTC 센서, microSD, 배터리 운용을 지원한다.

## SoC & 메모리

| 항목 | 값 |
|------|-----|
| SoC | Espressif ESP32-S3 (Xtensa LX7 듀얼코어, 최대 240MHz) |
| SRAM | 512KB |
| ROM | 384KB |
| PSRAM | 8MB (Octal SPI) |
| Flash | 16MB |

## 디스플레이

| 항목 | 값 |
|------|-----|
| 종류 | 4.2인치 반사형 LCD (RLCD), 모노크롬 |
| 해상도 | 400 × 300 |
| 컨트롤러 | **ST7305 / ST7306** (Sitronix 계열) — 자료마다 표기 상이 |
| 인터페이스 | SPI (SPIM2) |
| 백라이트 | 없음 (주변광 반사형, 초저전력) |

> 반사형 LCD는 전자종이와 유사하게 외부 광원을 반사해 표시하므로 백라이트가 없고
> 정적 화면 유지 전력이 매우 낮다. 갱신 속도는 일반 TFT보다 느리다.

## 오디오

| 구성 | 부품 |
|------|------|
| 코덱 (출력) | ES8311 저전력 코덱 |
| 마이크 ADC (입력) | ES7210, 듀얼 마이크 어레이 |
| 스피커 | 온보드 8Ω 2W + 외부 스피커 헤더 |
| 기능 | 노이즈 저감 / 에코 캔슬링 지원 |
| 인터페이스 | I2S (데이터) + I2C (코덱 제어) |

## 센서 & 타이밍

| 부품 | 기능 | I2C 주소 |
|------|------|----------|
| SHTC3 | 온도/습도 | 0x70 |
| PCF85063A | RTC (독립 전원 헤더 보유, INT=GPIO15) | 0x51 |

## 연결 & 입력

| 항목 | 값 |
|------|-----|
| 무선 | WiFi 802.11 b/g/n, Bluetooth 5.0 LE (최대 2Mbps) |
| USB | Type-C (전원/프로그래밍/충전), 네이티브 USB Serial/JTAG 컨트롤러 |
| 저장소 | microSD 슬롯 (SDMMC, 1-bit 버스) |
| 버튼 | Power, BOOT(GPIO0), USER(GPIO18) |
| 확장 | 2× 8핀 GPIO/UART/I²C 헤더 |
| 터치 | 정전식 터치 (I2C 버스) |

## 디지털 인터페이스 (ESP32-S3 일반 사양)

- 45× 프로그래머블 GPIO
- 4× SPI, 3× UART, 2× I2C, 2× I2S
- Full-speed USB OTG + USB Serial/JTAG
- 2× 12-bit SAR ADC (20채널)
- 14× 정전식 터치 I/O
- CAN (TWAI)

## 전원 & 배터리

| 항목 | 값 |
|------|-----|
| 입력 | 5V (USB Type-C) |
| 배터리 | 선택형 18650 리튬 홀더 + 충전 회로 |
| 측정 | 배터리 전압 분압 회로 → ADC 채널 3 |
| 표시등 | 충전/경고 LED |

## 물리 사양

- 크기: 92.5 × 70.1 × 13.5 mm (틸트 60°)
- 마운팅: 4× M2.5 홀

## 개발 프레임워크 지원

- **ESP-IDF** (이 저장소의 기본 선택) — [docs/esp-idf-development.md](esp-idf-development.md)
- Arduino IDE

## 비고

- Zephyr 보드 정의는 존재하나 "Not actively maintained" 상태로 표기됨.
- Waveshare 공식 wiki는 작성 시점 기준 내용이 비어있을 수 있어, 핀맵은 Zephyr
  devicetree를, 사양은 Waveshare/CNX 자료를 교차 확인했다. 원본은
  [docs/references.md](references.md) 참고.
