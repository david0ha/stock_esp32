# CLAUDE.md

이 저장소(`stock_esp32`)의 새 개발 타겟은 **Waveshare ESP32-S3-RLCD-4.2** 보드이며,
**ESP-IDF v6.0.1** 프레임워크로 개발한다. 이 문서는 다시 조사할 필요 없이 작업할 수 있도록
보드/툴체인 핵심 정보를 정리한 것이다. 상세 내용은 `docs/`를 참고한다.

## 빠른 시작 (가장 먼저 할 것)

ESP-IDF 환경 활성화 — **매 새 셸 세션마다 한 번 실행**해야 `idf.py`를 쓸 수 있다:

```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"
```

활성화 후 표준 워크플로:

```bash
idf.py set-target esp32s3      # 타겟 칩 지정 (프로젝트당 1회)
idf.py menuconfig              # Flash 16MB / PSRAM 8MB(Octal) 설정
idf.py build                   # 빌드
idf.py -p <PORT> flash monitor # 플래시 + 시리얼 모니터 (Ctrl+] 로 종료)
```

- `<PORT>`: macOS에서 보드 연결 시 보통 `/dev/cu.usbmodem*` (USB Serial/JTAG) 또는
  `/dev/cu.usbserial-*` (UART 브리지). `ls /dev/cu.*` 로 확인.
- 플래시 모드 진입이 안 되면 **BOOT 버튼을 누른 채 RESET**을 눌렀다 떼고 다시 시도.

> **프로젝트 구조**: 이 저장소 루트가 곧 ESP-IDF 프로젝트다 — `CMakeLists.txt`·`main/`·`components/`
> 가 루트에 있으니 `idf.py` 는 **저장소 루트에서** 실행한다. 펌웨어 앱은 `components/stock_core`
> (이식성 코어 + 디바이스 포트) + `components/user_app`(앱 글루), WiFi/티커를 캡티브 포털로
> 설정하는 `components/provisioning`(WiFi STA·SoftAP·NVS·SNTP `net_time` 포함), 보드 없이 UI를
> 확인하는 데스크톱 시뮬레이터는 `sim/`, 벤더링한 cJSON 은 `third_party/` 에 있다. 자세한 내용은
> [docs/stock-app.md](docs/stock-app.md).

## 타겟 보드 한눈에 보기

| 항목 | 사양 |
|------|------|
| SoC | ESP32-S3 (Xtensa LX7 듀얼코어, 최대 240MHz) |
| Flash / PSRAM | 16MB Flash / 8MB PSRAM (Octal) |
| 디스플레이 | 4.2" 반사형 흑백 LCD, 400×300, **ST7305/ST7306** 컨트롤러, SPI 연결, 백라이트 없음 |
| 터치 | 정전식 터치 (I2C) |
| 오디오 | ES8311 코덱 + ES7210 듀얼 마이크 ADC + 8Ω 2W 스피커 (I2S) |
| 센서/RTC | SHTC3 온습도(0x70), PCF85063A RTC(0x51) |
| 저장소 | microSD (SDMMC, 1-bit) |
| 무선 | WiFi 802.11 b/g/n, BLE 5.0 |
| USB | Type-C (전원/프로그래밍/충전), 네이티브 USB Serial/JTAG |
| 버튼 | Power, BOOT(GPIO0), USER(GPIO18) |
| 전원 | 5V USB-C, 선택형 18650 배터리 + 충전회로, 배터리 전압 ADC 측정 |

> 컨트롤러 표기: Zephyr devicetree는 `ST7306`, Waveshare/CNX 자료는 `ST7305`로 표기.
> 둘 다 Sitronix 계열 반사형 모노크롬 컨트롤러이며 드라이버 작성 시 둘 다 확인할 것.

핵심 핀맵(요약): SPI 디스플레이 `SCLK=11, MOSI=12, CS=40, DC=5, RST=41` ·
I2C `SDA=13, SCL=14` · I2S `MCLK=16, BCLK=9, WS=45, DOUT=10, DIN=8` ·
SD `CMD=21, CLK=38, D0=39` · UART0 `TX=43, RX=44`. 전체는 [docs/pinout.md](docs/pinout.md).

## 문서 구조 (docs/)

- [docs/board-hardware.md](docs/board-hardware.md) — 보드 전체 하드웨어 사양
- [docs/pinout.md](docs/pinout.md) — 페리페럴별 GPIO 핀 할당표
- [docs/esp-idf-development.md](docs/esp-idf-development.md) — ESP-IDF 설치/빌드/플래시/메뉴컨피그/드라이버 가이드
- [docs/examples.md](docs/examples.md) — `examples/`에 등록된 참고 예제(서브모듈) 설명
- [docs/graphics.md](docs/graphics.md) — 1-bit 그래픽 렌더링 방식(즉시모드 GFX vs LVGL) 및 선택 가이드
- [docs/simulator.md](docs/simulator.md) — 보드 없이 LVGL UI를 확인하는 데스크톱 시뮬레이터(`sim/`)
- [docs/references.md](docs/references.md) — 공식 문서/데이터시트 링크

## 참고 예제 (examples/, git submodule)

이 보드용 외부 예제를 서브모듈로 관리한다. 최초 클론 시 내용을 받으려면:

```bash
git submodule update --init --recursive
```

- `examples/waveshare-official` — Waveshare 공식 데모(Arduino/ESP-IDF/ESPHome/XiaoZhi) ⭐
- `examples/VolosR-waveshareLRCL` — Volos 시계/날씨 (Arduino)
- `examples/VolosR-eBikeRLCD` — Volos E-Bike 대시보드 (Arduino)
- `examples/JasonHEngineering-RLCD-clock` — 스마트 시계, 오디오/RTC 포함 (Arduino)

상세 및 영상/블로그 링크는 [docs/examples.md](docs/examples.md). 예제는 Arduino 기반이므로
핀맵·드라이버·UI 패턴을 참고하되 빌드는 ESP-IDF로 옮겨 적용한다.

## 저장소 내 기존 프로젝트 (참고용 / 다른 보드)

아래 3개는 이전 작업물로, **타겟 보드(ESP32-S3-RLCD-4.2)와는 다른 하드웨어**다. 패턴 참고용.

- `ESP32_Stock_Ticker/` — Arduino 기반 멀티 종목 티커 (Finnhub API, 자체 PCB)
- `espstock/` — ESPHome 8-세그먼트 주가 디스플레이
- `finance-monitor/` — PlatformIO/Arduino, ESP32-2432S028R(Cheap Yellow Display, ILI9341), Yahoo Finance + RSS

## 작업 규칙

- ESP-IDF 명령 전에는 항상 위 `source ...activate_idf_v6.0.1.sh`가 선행되어야 한다.
- 디스플레이/오디오/센서 드라이버 추가 시 [docs/pinout.md](docs/pinout.md)의 GPIO와
  [docs/esp-idf-development.md](docs/esp-idf-development.md)의 컴포넌트 가이드를 먼저 확인한다.
- 보드 사양에 불확실함이 있으면 추측하지 말고 [docs/references.md](docs/references.md)의 원본을 확인한다.
