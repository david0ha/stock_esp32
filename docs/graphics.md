# 그래픽 렌더링 방식 — RLCD 4.2" (400×300, 1-bit 흑백)

이 보드는 **흑백 1bpp**(회색 없음) 반사형 패널이다. 예제들이 그래픽을 어떻게 그리는지
두 갈래로 정리한다. 우리 저장소(ESP-IDF) 개발 시 어느 쪽을 가져올지 판단 기준도 적는다.

핀맵: SPI `MOSI=12, SCLK=11, DC=5, CS=40, RST=41` (→ [pinout.md](pinout.md)).

---

## 방식 A — 1-bit 프레임버퍼 + 즉시모드 GFX (Volos eBikeRLCD)

`examples/VolosR-eBikeRLCD/espRLCD/` 의 실제 동작 구조:

### 핵심 파이프라인
```
오프스크린 1-bit 캔버스에 매 프레임 전부 그림
        ↓ (픽셀 단위 복사)
RLCD 패널 버퍼 (display_bsp)
        ↓ (SPI 일괄 전송)
화면 갱신
```

1. **오프스크린 캔버스 (RAM, 1bpp)**
   ```cpp
   TFT_eSprite canvas = TFT_eSprite(&tft);
   canvas.setColorDepth(1);        // 1비트
   canvas.createSprite(400, 300);  // 400×300 = 15,000바이트 버퍼
   canvas.setTextDatum(4);         // 텍스트 중앙정렬
   ```
   - 실제 패널이 아니라 메모리상의 1비트 버퍼에 그린다(더블버퍼링 → 깜빡임 없음).

2. **즉시모드로 매 프레임 전체 다시 그림** (`draw()`)
   ```cpp
   canvas.fillRect(0,0,400,300,0);          // 화면 클리어(검정)
   canvas.fillCircle(cx, cy, r+12, 0);      // 게이지 원
   canvas.drawArc(cx,cy,r+2,r-2,70,290,1,0,0);   // 눈금 호
   canvas.drawWideLine(...);  canvas.drawWedgeLine(...);  // 두꺼운/쐐기 선
   canvas.fillTriangle(...);  canvas.fillRoundRect(...);  // 방향지시등·온도계 아이콘
   ```
   - 쓰는 프리미티브: `fillRect / fillCircle / fillTriangle / fillRoundRect /
     drawArc / drawWideLine / drawWedgeLine / drawString`.
   - 색은 `0`(검정)·`1`(흰)만. 그라데이션 불가 → 필요하면 디더링 패턴을 직접 그려야 함.

3. **커스텀 폰트** — TTF를 변환한 헤더(`smallFont.h`, `valueFont.h`, `midleFont.h`)를
   런타임에 스위칭:
   ```cpp
   canvas.loadFont(valueFont);
   canvas.drawString(String(speed), cx, 270);
   canvas.unloadFont();
   ```

4. **게이지 = 삼각함수 좌표 사전계산** (`setup()`에서 1회)
   ```cpp
   double rad = 0.01745;            // deg→rad
   for (int i=0;i<225;i++){         // 각도별 점 좌표 미리 계산
     x[i]=(r-4)*cos(rad*a)+cx;  y[i]=(r-4)*sin(rad*a)+cy;   // 눈금 바깥점
     nx[i]=(r-60)*cos(rad*a)+cx; ny[i]=(r-60)*sin(rad*a)+cy; // 바늘 끝점
     a++; if(a==360) a=0;
   }
   ```
   - 루프에서는 배열 룩업만 → 바늘은 `drawWedgeLine(nx[speed],ny[speed],cx,cy,2,8,1)`.
   - 매 프레임 `cos/sin` 호출을 피해 CPU 절약(반사형 패널은 갱신이 느리므로 중요).

5. **캔버스 → 패널 푸시** (`pushCanvasToRLCD()`)
   ```cpp
   uint8_t *buf = canvas.getPointer();        // 1비트 버퍼 직접 접근
   int bytesPerRow = (400 + 7) / 8;
   RlcdPort.RLCD_ColorClear(ColorWhite);
   for (y..) for (bit..) if (비트 셋) RlcdPort.RLCD_SetPixel(x, y, ColorBlack);
   RlcdPort.RLCD_Display();                   // SPI로 패널에 일괄 전송
   ```

6. **애니메이션** — 상태 카운터로 처리 (예: 방향지시등 깜빡임 `ind_ani`, 속도 `speed`),
   `loop()`에서 값 갱신 후 `draw()` 재호출.

### ⭐ ESP-IDF 재사용 포인트
`display_bsp.h`/`.cpp`(DisplayPort 클래스)는 **순수 ESP-IDF 드라이버**다
(`driver/spi_master.h`, `esp_lcd_panel_io.h` 사용, Arduino 의존 없음). 즉:
- **드라이버는 그대로 ESP-IDF 프로젝트로 가져올 수 있다.** ST7305/ST7306 초기화 +
  픽셀 LUT 최적화(`AlgorithmOptimization 3`, 룩업테이블로 x,y→버퍼 비트 매핑)까지 포함.
- Arduino에 묶인 건 스케치 쪽의 `TFT_eSprite` 캔버스뿐. ESP-IDF에서는 이를
  **LVGL의 1bpp 버퍼**나 **자체 1비트 버퍼 + Adafruit_GFX `GFXcanvas1`**로 대체하면 된다.
- (참고: `VolosR-waveshareLRCL`는 동일 패턴이되 캔버스로 `GFXcanvas1`을 써서 더 가볍다.)

---

## 방식 B — LVGL (Draeger-IT Part 2 = Waveshare 공식과 동일 구조)

Draeger-IT **Part 1**은 언박싱(코드 없음), **Part 2**가 Arduino IDE + **LVGL** 구동법이다.
그런데 Part 2가 쓰는 구조(`display_bsp.h` + `lvgl_bsp.h` + 임계값 flush)는 **Waveshare 공식
저장소의 예제와 사실상 동일**하다. 게다가 공식 저장소엔 **ESP-IDF용 LVGL 예제가 그대로
들어있어** 우리 타겟(ESP-IDF)에 바로 가져다 쓸 수 있다:

- `examples/waveshare-official/02_Example/ESP-IDF/09_LVGL_V9_Test` (LVGL 9)
- `examples/waveshare-official/02_Example/ESP-IDF/08_LVGL_V8_Test` (LVGL 8)
- Arduino판: `02_Example/Arduino/09_LVGL_V9_Test` 등 (Draeger Part 2가 이 계열)

### 컴포넌트 구조 (공식 ESP-IDF 09_LVGL_V9_Test 기준)
```
main/                  app_main: 드라이버 init → LVGL init → UI init → task 시작
components/
├─ port_bsp/display_bsp  ST7305/6 패널 드라이버 (방식 A와 동일한 esp_lcd SPI 드라이버)
├─ app_bsp/lvgl_bsp      LVGL 포팅: init, PSRAM 더블버퍼, tick 타이머, 전용 태스크+뮤텍스
├─ ui_bsp/               UI 코드 — GUI Guider로 생성 (gui_guider.c, setup_scr_screen.c …)
└─ user_app/             앱 글루
```
LVGL은 **컴포넌트 매니저로 자동 설치**된다(`main/idf_component.yml`):
```yaml
dependencies:
  lvgl/lvgl: ^9.4.0
```

### 핵심 1 — flush 콜백에서 RGB565 → 1-bit 임계값 변환
LVGL은 내부적으로 **RGB565(16비트 컬러)** 로 렌더링하고, 패널에 내보낼 때 콜백에서
픽셀별 임계값으로 흑백을 정한다 (`main/main.cpp`):
```cpp
static void Lvgl_FlushCallback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map) {
    uint16_t *buffer = (uint16_t *)color_map;
    for (int y = area->y1; y <= area->y2; y++)
        for (int x = area->x1; x <= area->x2; x++) {
            uint8_t color = (*buffer < 0x7fff) ? ColorBlack : ColorWhite; // 중간값 기준 이진화
            RlcdPort.RLCD_SetPixel(x, y, color);
            buffer++;
        }
    RlcdPort.RLCD_Display();          // SPI 일괄 전송
    lv_disp_flush_ready(drv);
}
```
> 단순 임계값이라 **계조/디더링 없음**. 그라데이션이 필요하면 여기서 디더링을 넣거나,
> LVGL의 네이티브 1bpp 포맷(`LV_COLOR_FORMAT_I1`)을 쓰는 방법도 있다(메모리 절약).

### 핵심 2 — 버퍼/태스크/틱 (`lvgl_bsp.cpp`)
```cpp
lv_init();
lv_display_t *disp = lv_display_create(width, height);
lv_display_set_flush_cb(disp, flush_cb);
// 풀프레임 더블버퍼를 PSRAM에 할당 (RGB565: 400*300*2 ≈ 240KB ×2)
buffer_1 = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
buffer_2 = heap_caps_malloc(buffer_size, MALLOC_CAP_SPIRAM);
lv_display_set_buffers(disp, buffer_1, buffer_2, buffer_size, LV_DISPLAY_RENDER_MODE_FULL);
// esp_timer로 lv_tick_inc() 주기 호출, 전용 FreeRTOS 태스크에서 lv_timer_handler() 구동
```
- **풀리프레시 모드**(`RENDER_MODE_FULL`) — 갱신 느린 반사형 패널에 적합.
- LVGL 작업은 **전용 태스크 + 뮤텍스(`Lvgl_lock/unlock`)** 로 보호 → 다른 태스크에서
  UI를 만질 때 thread-safe.

### 핵심 3 — 폰트 & 보드 설정 (Draeger 블로그가 특히 강조)
- LVGL은 기본적으로 대부분 폰트를 끈다. `lv_conf.h`에서 필요한 것만 켜야 함:
  `LV_FONT_MONTSERRAT_12 1` … `LV_FONT_MONTSERRAT_48 1`.
- 보드/빌드 설정(누락 시 부팅 assert): **Flash QIO 80MHz / 16MB**, **Octal(OPI) PSRAM**,
  `FREERTOS_HZ=1000`, (Arduino는 **USB CDC On Boot = Enabled**). 공식 예제
  `sdkconfig.defaults`에 동일하게 들어있음:
  ```
  CONFIG_ESPTOOLPY_FLASHMODE_QIO=y
  CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
  CONFIG_SPIRAM=y
  CONFIG_SPIRAM_MODE_OCT=y
  CONFIG_SPIRAM_SPEED_80M=y
  CONFIG_FREERTOS_HZ=1000
  ```

### UI 작성 — GUI Guider
공식 예제의 `ui_bsp/generated/`는 **NXP GUI Guider**(드래그&드롭 UI 툴)가 생성한 코드다.
화면을 GUI로 디자인하면 LVGL C 코드가 자동 생성되므로, 손으로 위젯 코드를 안 짜도 된다.

---

## 우리 저장소(ESP-IDF)에서 그래픽을 쓸 때 — 선택 가이드

| 목적 | 추천 | 근거 |
|------|------|------|
| 계기판/대시보드처럼 **직접 그리는 커스텀 그래픽** | **방식 A** | `display_bsp`(ESP-IDF 네이티브) + 1bpp 캔버스. 가볍고 제어가 명확. Volos 게이지 패턴 그대로 이식 가능 |
| 버튼·리스트·차트·터치 등 **위젯 기반 UI** | **방식 B (LVGL)** | 공식 LVGL 포팅 + esp_lcd 활용. `waveshare-official/02_Example/ESP-IDF` 먼저 확인 |

공통 원칙 (1bpp 패널):
- 색은 흑/백뿐 — 음영은 **디더링(점/해칭 패턴)** 으로 표현.
- 반사형이라 갱신이 느림 → **오프스크린 버퍼에 그린 뒤 한 번에 푸시**, 변하지 않는
  요소는 매 프레임 재계산하지 말고 **사전계산/부분갱신**으로 CPU·전력 절약.
- 폰트는 헤더로 변환한 비트맵 폰트(GFX) 또는 LVGL 폰트 사용.

> ESP-IDF 빌드/드라이버 컴포넌트 일반 사항은 [esp-idf-development.md](esp-idf-development.md) 참고.
