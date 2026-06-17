# 홈 화면 "다음 중요도3 경제이벤트" 행 — 설계

작성일: 2026-06-18
브랜치: `worktree-fix+moreclock`

## 1. 목적

400×300 반사형 모노 패널의 **홈(메인) 화면**에, 현재 시각 기준으로 **다음에 발생할
중요도 3(HIGH) 경제 캘린더 이벤트** 한 건을 한 행으로 표시한다. 위치는 주식 정보행
(심볼/가격/변동률, 구분선 y=170)과 온도/습도/배터리 strip(바닥) **사이**의 빈 공간이다.

시간이 흐르면 표시된 이벤트가 지나가고, 별도 조작 없이 자동으로 그 다음 중요도3 이벤트로
전진한다.

## 2. 범위

### 가져오는 것 (econ-calendar-week-paging → 이 브랜치, 데이터 계층만)

- `components/stock_core/include/econ_model.h` — `econ_event_t`, `econ_calendar_t`,
  `econ_impact_t` (NONE=0, LOW=1, MEDIUM=2, HIGH=3)
- `components/stock_core/include/econ_parse.h` + `econ_parse.c` — 순수 JSON/시간 헬퍼
  (`econ_parse_calendar`, `econ_week_range`, `econ_local_tz_off`, `econ_ymd_to_epoch`,
  `econ_impact_from_str`). 네트워크/전역 없음, 호스트 테스트 대상.
- `components/stock_core/include/econ_service.h` + `econ_service.c` —
  `econ_service_fetch()`: URL 구성 → `http_get` 포트 → 파싱.
- `components/stock_core/Kconfig.projbuild` — `STOCK_FMP_API_KEY`, `STOCK_ECON_BASE_URL`
  항목 추가.
- `components/stock_core/CMakeLists.txt` — `econ_parse.c`, `econ_service.c` SRCS 등록.
- 호스트 테스트: `components/stock_core/test/host/test_econ.c` + 픽스처
  `fixtures/fmp_econ.json`, `test/host/CMakeLists.txt`에 타깃 등록 (UBSan, ASan 금지).

### 새로 추가하는 순수 헬퍼 (econ_parse.c, 호스트 테스트 대상)

- `int econ_next_after(const econ_calendar_t *cal, int64_t now_utc);`
  items[]는 ts 오름차순 정렬이므로 `ts > now_utc`인 첫 항목 인덱스를 선형 스캔으로 반환,
  없으면 -1.
- `void econ_when_label(int64_t ts, time_t now, long tz_off, char *out, size_t n);`
  디바이스 로컬 기준 상대일 라벨 생성:
  - 같은 날 → `"TODAY HH:MM"`
  - 다음 날 → `"TOMORROW HH:MM"`
  - 그 외 7일 이내 → `"<WD> HH:MM"` (WD = MON/TUE/…)
  - 그 이상 → `"MM-DD HH:MM"`

### 가져오지 않는 것 (YAGNI — 요청은 홈 행 1개)

- 별도 전체 캘린더 화면(`ui_econ.c`, `ui_econ.h`), KEY+BOOT 진입, 주차 페이징
- `buttons.c`/`buttons.h`의 econ 관련 변경, `stock_text.h`
- `tools/econ_proxy/` 배포물, `econ_service` 호스트 테스트(`test_econ_service.c`)는
  http 목 인프라 필요 시 보류 — 우선 순수 파서 테스트만 이식하고, 목이 손쉽게 붙으면 포함.

## 3. 데이터 가져오기 — 새 EconTask

StockTask(입력/렌더 전용, 고우선)는 절대 네트워크에서 블록하지 않는다는 기존 원칙을
유지한다. 경제 캘린더 fetch는 별도 저우선 태스크에서 수행한다.

- **EconTask** (신규, `user_app.cpp`): 큰 스택(TLS+JSON), 저우선.
  1. 이번 주(`week_offset=0`)를 `min_impact = ECON_IMPACT_HIGH(3)`로 `econ_service_fetch`.
     - `tz_off`는 `econ_local_tz_off(now)`로 계산(홈 시계와 동일 TZ).
     - `fmp_key`는 `CONFIG_STOCK_FMP_API_KEY`.
  2. 결과에 `econ_next_after(now) < 0`(미래 high 이벤트 없음)이면 다음 주(`+1`)도 fetch해
     그 결과를 사용.
  3. 성공 시 공유 캐시 `s_econ`(econ_calendar_t)에 mutex 보호하에 복사.
  4. `ECON_REFRESH_SECONDS`(기본 3600s)마다 반복 — actual 값/일정 변경 반영.
  5. 캐시에 미래 high가 없어지면(소진) 조기 재fetch.
- **메모리**: `econ_calendar_t`는 약 7KB. 공유 캐시 `s_econ`과 EconTask용 스크래치 1개를
  PSRAM(`heap_caps_*`, `MALLOC_CAP_SPIRAM`)에 할당. 스택에 7KB 올리지 않는다.
- **동기화**: econ 캐시 전용 `s_econ_mtx` 추가(기존 `s_mtx`와 분리해 결합도↓).

## 4. 표시 — 홈 행 (2줄 형식)

`components/stock_core/ui_home.c`에 라벨 2개와 구분선 1개 추가. y=170 구분선 아래:

```
 1 day + 1.24%
 ---------------------------   (기존 rule, y=170)
 TODAY 21:30 · US        ***   (캡션, F_CAP 14px)
 Nonfarm Payrolls              (이벤트명, F_TEXT 26px 굵게, 폭 초과 시 … 잘림)
 ---------------------------   (신규 rule, ~y=240)
 21.5°C   62%        BATTERY   (기존 strip 유지)
```

- 캡션: `econ_when_label()` 결과 + `country` + 중요도 마커.
- 좌표(잠정, 구현 시 미세 조정): 캡션 `LV_ALIGN_TOP_LEFT (16, 178)`,
  이벤트명 `LV_ALIGN_TOP_LEFT (16, 200)`, 신규 rule `rule(page, 240, 12, 2)`.
  바닥 strip은 그대로(값 y≈262, 라벨 y≈284).
- 이벤트명은 라벨 폭을 화면폭-32로 고정하고 `LV_LABEL_LONG_DOTS`로 말줄임.

### 글리프 안전성

1-bpp 커스텀 폰트(`ui_font_black_26`)와 `lv_font_montserrat_14`에 `★`(U+2605)·`·`(U+00B7)가
없을 수 있다. 구현 시 폰트 글리프 커버리지를 확인하고:
- 중요도 마커는 캘린더 화면 선례대로 ASCII `***` 사용.
- 구분점은 `·`가 렌더되면 유지, 아니면 ASCII(` | ` 또는 공백 2칸)로 폴백.
- 기존 코드가 `°`(`\xC2\xB0`)를 `ui_font_black_26`로 출력 중이므로 최소한의 라틴-1 보조
  글리프는 존재함을 참고.

### 새 UI API

- `ui_home.c`: `void ui_home_set_econ(const econ_event_t *ev, bool valid);`
  - `valid && ev` → 캡션/이벤트명 채움. 아니면 두 라벨을 빈 문자열로(행을 클린하게 비움).
- `ui_stock.c`: `void ui_stock_update_econ(const econ_event_t *ev, bool valid);`
  홈 페이지로 패스스루. (`ui_stock.h`에 선언 추가)

## 5. 갱신 경로 (자동 전진)

- StockTask의 기존 idle 틱(`HOME_TICK_SECONDS`=15s)과 홈 페이지 진입 시,
  `tick_home_env()` 옆에 **`tick_home_econ()`** 호출 추가:
  1. `s_econ_mtx` 잠금 후 `s_econ`에서 `econ_next_after(now)` 계산.
  2. 인덱스 ≥ 0이면 해당 `econ_event_t`와 valid=true, 아니면 valid=false로
     `ui_stock_update_econ()` 호출(LVGL 락 하에).
  3. 미래 high가 없으면 EconTask에 조기 재fetch 신호(세마포어/플래그).
- 효과: 한 이벤트가 지나가면 네트워크 호출 없이 즉시 다음 중요도3 이벤트로 전진하고,
  `TODAY/TOMORROW` 라벨도 매 틱 정확히 유지된다.

## 6. 설정 (Kconfig)

- `STOCK_FMP_API_KEY` (string, 기본 "") — FMP/프록시 인증 키.
- `STOCK_ECON_BASE_URL` (string, 기본 FMP `https://financialmodelingprep.com/stable/economic-calendar`)
  — 메모리 기록상 FMP 무료 키는 econ에서 HTTP 402이므로, 실사용은 investing.com 프록시
  URL(`tools/econ_proxy`)을 sdkconfig에 설정. 본 작업은 프록시 툴을 가져오지 않으나
  URL 설정만으로 동작.
- 중요도 필터는 홈 행 한정 **HIGH(3) 고정** — `econ_service_fetch` 호출 시 인자로 전달하며
  별도 Kconfig 불필요(`STOCK_ECON_MIN_IMPACT`는 가져오지 않음).

## 7. 에러/엣지 케이스

- FMP 키 없음 / 네트워크 실패 / 파싱 실패 → `s_econ.valid=false` → 홈 행 빈 채(클린).
- 이번 주·다음 주 모두 미래 high 이벤트 없음 → 행 빈 채.
- 이벤트명 과길이 → LVGL 말줄임(`…`).
- 시계(TZ) 미설정 상황: 홈 시계가 이미 로컬시간을 그리므로 TZ는 설정돼 있다는 전제.
  `econ_local_tz_off`가 같은 TZ를 사용해 일관.

## 8. 검증

- **호스트 테스트(UBSan, ASan 금지)**: `econ_next_after`(빈/전부 과거/경계 ts), 
  `econ_when_label`(TODAY/TOMORROW/요일/날짜 분기), 이식한 `econ_parse_calendar` 픽스처.
- **시뮬레이터(`sim/`)**: 더미 `econ_event_t`로 홈 행 2줄 렌더 및 말줄임 확인.
- **빌드**: `source .../activate_idf_v6.0.1.sh` 후 `idf.py build`.

## 9. 영향 파일 요약

| 파일 | 변경 |
|------|------|
| `components/stock_core/include/econ_model.h` | 이식(신규) |
| `components/stock_core/econ_parse.{c,h}` | 이식 + `econ_next_after`/`econ_when_label` 추가 |
| `components/stock_core/econ_service.{c,h}` | 이식(신규) |
| `components/stock_core/ui_home.c` | econ 라벨 2개 + rule + `ui_home_set_econ` |
| `components/stock_core/include/ui_home.h` | `ui_home_set_econ` 선언 |
| `components/stock_core/ui_stock.c` | `ui_stock_update_econ` 패스스루 |
| `components/stock_core/include/ui_stock.h` | 선언 추가 |
| `components/stock_core/CMakeLists.txt` | econ .c 등록 |
| `components/stock_core/Kconfig.projbuild` | FMP 키 + base URL |
| `components/stock_core/test/host/*` | econ 파서 테스트 이식 + 신규 헬퍼 테스트 |
| `components/user_app/user_app.cpp` | EconTask + `tick_home_econ` + 캐시/뮤텍스 |
| `sim/*` | 더미 econ 렌더(확인용) |
