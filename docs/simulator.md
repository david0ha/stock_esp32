# LVGL 시뮬레이터 (보드 없이 UI 확인)

ESP32-S3-RLCD-4.2 의 LVGL UI를 **보드 연결 없이** macOS에서 렌더링해 확인하는 도구.
위치: `firmware/lvgl_sim/`.

## 무엇을 하나

- ESP-IDF 예제(`firmware/lvgl_v9_test`)의 **LVGL 소스와 GUI Guider UI를 그대로 재사용**한다.
- 디바이스의 `Lvgl_FlushCallback` 과 **동일한 흑백 이진화 규칙**(`픽셀 < 0x7FFF ? 검정 : 흰색`)을
  적용 → 반사형 흑백 패널에 실제로 보일 모습을 그대로 재현한다.
- 헤드리스로 렌더링해 **PNG 스크린샷**으로 저장(GUI 창 불필요).

## 사용법

```bash
cd firmware/lvgl_sim
./sim.sh        # 빌드 → 실행 → shots/sim_frame1.png, sim_frame2.png 생성
```

수동 단계:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/sim shots                       # BMP 출력
sips -s format png shots/sim_frame1.bmp --out shots/sim_frame1.png
```

## 구성

| 파일 | 역할 |
|------|------|
| `main_sim.c` | 헤드리스 LVGL 디스플레이 + RGB565→흑백 + BMP 저장 |
| `lv_conf.h` | LVGL 설정 (색심도 16=디바이스 동일, snapshot/log on, CLIB malloc) |
| `CMakeLists.txt` | LVGL(예제 managed_components) + ui_bsp 소스를 묶어 빌드 |
| `sim.sh` | 빌드+실행+PNG 변환 한 번에 |

## 현재 예제(09_LVGL_V9_Test)의 UI

`screen_img_1`(곰), `screen_img_2`(고양이) 두 전체화면 이미지가 1.5초마다 교대.
`main_sim.c`가 각 이미지를 표시한 상태로 2장을 캡처한다.

## 알아둘 점 / 한계

- **이진화 규칙이 거칠다**: 디바이스가 쓰는 `px < 0x7FFF` 단순 임계값이라, 컬러/중간톤
  이미지(곰)는 거의 검정 실루엣으로 나온다. 사진(고양이)은 대비가 커서 흑백이 잘 나뉜다.
  → 더 나은 모노 표현을 원하면 **디더링**이나 **루미넌스 임계값**으로 바꿔야 한다
  (이건 디바이스 펌웨어의 flush 콜백을 함께 바꿔야 실물과 일치).
- 시뮬레이터는 **LVGL UI 레이어만** 재현한다. 오디오/센서/SD/실제 SPI 타이밍/반사형 패널의
  물리적 명암은 실물 보드에서만 확인 가능.
- UI 코드(`ui_bsp`)는 하드웨어 비종속이므로, 보드용 펌웨어와 **같은 UI 소스**를 공유해
  디자인을 빠르게 반복할 수 있다.

## 버튼/상태 시뮬레이션

현재 예제 UI에는 버튼이 없어 두 이미지 토글만 캡처한다. 버튼/위젯이 추가되면
`main_sim.c`에서 (a) 위젯 상태를 직접 바꾸거나 (b) `lv_indev` 입력을 주입한 뒤
스냅샷을 떠서 상태별 화면을 캡처할 수 있다.

## (선택) 인터랙티브 SDL 창

지금은 헤드리스 스크린샷 방식이다. 마우스/키보드로 **직접 클릭**하는 실시간 창이 필요하면
LVGL 9의 SDL 드라이버(`lv_sdl_window` + `lv_sdl_mouse`)로 창 모드를 추가할 수 있다
(SDL2 설치됨: `/opt/homebrew/opt/sdl2`). 필요 시 요청.
