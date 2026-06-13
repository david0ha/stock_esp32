# 참고 자료 — ESP32-S3-RLCD-4.2

이 보드 관련 정보의 원본 링크. 사양/핀맵에 의문이 생기면 여기서 재확인한다.

## 보드 문서

- **Zephyr 보드 문서** (사양 + 프로그래밍/디버깅 개요)
  https://docs.zephyrproject.org/latest/boards/waveshare/esp32s3_rlcd_4_2/doc/index.html
- **Zephyr 보드 소스** (devicetree·pinctrl = 핀맵의 1차 출처)
  https://github.com/zephyrproject-rtos/zephyr/tree/main/boards/waveshare/esp32s3_rlcd_4_2
  - `esp32s3_rlcd_4_2-pinctrl.dtsi` — UART/SPI/I2C/I2S/SDMMC 핀
  - `esp32s3_rlcd_4_2_esp32s3_procpu.dts` — 디바이스/주소/버튼/ADC
- **Waveshare 공식 문서** (작성 시점 내용 비어있을 수 있음)
  https://docs.waveshare.com/ESP32-S3-RLCD-4.2
- **CNX Software 소개 기사** (사양·오디오 구성)
  https://www.cnx-software.com/2026/01/06/esp32-s3-development-board-features-4-2-inch-reflective-lcd-rlcd-dual-microphone-array-onboard-speaker/

## ESP-IDF

- ESP-IDF 프로그래밍 가이드 (ESP32-S3)
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/
- esp_lcd (디스플레이 추상화)
  https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/peripherals/lcd/index.html
- ESP Component Registry (드라이버 검색: ST7305/ST7306, ES8311, ES7210 등)
  https://components.espressif.com/
- esp_codec_dev (오디오 코덱 드라이버)
  https://components.espressif.com/components/espressif/esp_codec_dev

## 칩/부품 데이터시트

- ESP32-S3 데이터시트: https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf
- ST7305 / ST7306 (Sitronix 반사형 모노크롬 LCD 컨트롤러) — 제조사 데이터시트 검색
- ES8311 (오디오 코덱), ES7210 (마이크 ADC) — Everest Semi 데이터시트
- SHTC3 (Sensirion 온습도), PCF85063A (NXP RTC) — 제조사 데이터시트

## 로컬 환경

- ESP-IDF v6.0.1 설치 경로: `~/.espressif/v6.0.1/`
- 활성화 스크립트: `source "~/.espressif/tools/activate_idf_v6.0.1.sh"`
