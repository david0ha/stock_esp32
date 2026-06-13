# 참고 예제 모음 (examples/)

ESP32-S3-RLCD-4.2 보드용 외부 예제들을 **git submodule**로 `examples/` 아래에 등록했다.
(원본 저장소의 히스토리를 그대로 유지하면서, 이 저장소 커밋 시 충돌/경고가 없도록 관리)

## 서브모듈 사용법

```bash
# 최초 클론 시 / 비어있을 때 내용 가져오기
git submodule update --init --recursive

# 모든 예제 최신화
git submodule update --remote
```

각 예제는 독립된 저장소이므로, 해당 폴더 안에서 별도로 `git pull` 등을 할 수 있다.
이 저장소 입장에서는 "특정 커밋을 가리키는 포인터"만 추적하므로 안전하다.

---

## 1. `examples/waveshare-official` — Waveshare 공식 데모 ⭐

- 출처: https://github.com/waveshareteam/ESP32-S3-RLCD-4.2
- 가장 권위 있는 1차 자료. 보드 제조사 공식 예제/라이브러리/펌웨어.
- 구조:
  - `01_Arduino_Libraries/` — SensorLib, U8g2, LVGL8, LVGL9 (의존 라이브러리)
  - `02_Example/` — **Arduino / ESP-IDF / ESPHome / XiaoZhi** 4종 예제
  - `03_Firmware/` — 사전 빌드 펌웨어(`01_Factory_V1.bin`, `02_XiaoZhi_V2.1.0.bin`)
  - `Tools-Configuration.png` — Arduino IDE 보드 설정 레퍼런스
- **ESP-IDF 개발 시 `02_Example/ESP-IDF`를 1순위 참고**한다. 디스플레이/오디오/센서
  드라이버의 공식 구현을 여기서 확인할 수 있다.

## 2. `examples/VolosR-waveshareLRCL` — Volos 시계/날씨 데모

- 출처: https://github.com/VolosR/waveshareLRCL
- 영상: https://www.youtube.com/watch?v=MziW1InJwa4
- **Arduino** (`espRLCD/espRLCD.ino`). 커스텀 `display_bsp`(DisplayPort 클래스)로
  ST7305/ST7306 패널을 SPI 구동, `Adafruit_GFX` + `GFXcanvas1`(1bpp)로 렌더링.
- 핀맵 확인용으로 매우 유용: `DisplayPort(12,11,5,40,41,W,H)`
  = **MOSI=12, SCLK=11, DC=5, CS=40, RST=41**, 배터리 ADC=GPIO4, SHTC3=0x70, PCF85063A RTC.
- 햇빛 가독성 강조한 시계/온습도/배터리 표시 예제.

## 3. `examples/VolosR-eBikeRLCD` — Volos E-Bike 대시보드

- 출처: https://github.com/VolosR/eBikeRLCD
- 영상: https://www.youtube.com/watch?v=EKnZ7ZisUj4
- **Arduino** (`espRLCD/espRLCD.ino`). waveshareLRCL과 동일한 `display_bsp` 기반.
- 큰 글꼴(`bigFont`/`midleFont`/`smallFont`/`valueFont`)로 속도/주행 데이터를
  햇빛에서도 잘 보이게 표시하는 대시보드 UI 패턴 참고용.

## 4. `examples/JasonHEngineering-RLCD-clock` — 스마트 시계 (오디오/RTC 포함)

- 출처: https://github.com/JasonHEngineering/waveshare_RLCD_400x300_monochrome
- 영상: https://www.youtube.com/watch?v=kfiwg6aNjsw
- 블로그: https://jashuang1983.wordpress.com/waveshare-4-2-rlcd-customized-monochrome-smart-clock/
- **Arduino**. V1/V2 두 버전 + 3D 프린트 STEP 파일 + 모노크롬 이미지 생성 Python 스크립트.
- 포함 드라이버: `display_bsp`, **`codec_bsp`(ES8311/ES7210 오디오)**,
  **`PCF85063A`(RTC)** — 오디오/RTC 연동 레퍼런스로 특히 유용.
- 기능: 인터넷 시간 동기화, 버스 도착시간, 트리비아, 오디오 명령 기반 ChatGPT 질의
  (일부 기능은 싱가포르 현지화).

---

## 영상/블로그만 있는 자료 (코드 저장소 없음)

서브모듈로 등록하지 않았지만 참고할 만한 자료:

- **Draeger-IT (Part 1)** — 언박싱/첫인상 ("ePaper killer?")
  - 영상: https://www.youtube.com/watch?v=TG7722V8UGw
  - 블로그: https://draeger-it.blog/esp32-s3-4-2-display-epaper-killer-unboxing/
- **Draeger-IT (Part 2)** — Arduino IDE + **LVGL** 구동법
  - 영상: https://www.youtube.com/watch?v=-ohhYlIscEo
  - 블로그: https://draeger-it.blog/esp32-s3-rlcd-display-ansteuern-so-klappt-die-programmierung-mit-arduino-ide-teil-2/

---

## 참고: 프레임워크 메모

- 위 외부 예제 4종은 모두 **Arduino** 기반이다. 이 저장소의 자체 개발은 **ESP-IDF**를
  사용하므로(→ [esp-idf-development.md](esp-idf-development.md)), 예제의 **핀맵·드라이버
  로직·UI 패턴**을 참고하되 빌드 시스템은 ESP-IDF로 옮겨 적용한다.
- ESP-IDF 네이티브 구현이 필요하면 `examples/waveshare-official/02_Example/ESP-IDF`를 먼저 본다.
