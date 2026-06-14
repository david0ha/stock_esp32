# 주식 모니터 앱 (Stock Monitor)

개인 투자자용 ESP32 주가 모니터. 상단 고정바(티커/현재가/증감%)와 3초마다 순환하는
하단 영역(① 당일 분봉 차트 ② 뉴스 헤드라인 ③ 투자지표)으로 구성된다.
LVGL v9로 그리며, 400×300 반사형 흑백 패널에 맞춰 순흑백·대형 폰트로 디자인했다.

## 아키텍처 — 한 곳에만 플랫폼 의존성

```
components/stock_core/                 (대부분 플랫폼 비종속, 펌웨어/시뮬레이터/테스트 공유)
├── include/stock_model.h    데이터 모델 (quote / series / metrics / news)
├── stock_parse.c            JSON → 모델 파서 (순수 함수, 단위테스트 대상)
├── stock_service.c          URL 조립 + http_get 호출 + 파싱 오케스트레이션
├── include/http_port.h      ★ 유일한 플랫폼 경계: char *http_get(url, *status)
├── ui_stock.c               LVGL UI (상단바 + 3페이지 + 페이지 점)
├── http_port_esp.c          [펌웨어 전용] esp_http_client + TLS 인증서 번들 (PSRAM 누적)
└── Kconfig.projbuild        사용자 설정(아래)

components/provisioning/               [펌웨어 전용] WiFi 연결 + 티커 프로비저닝
├── prov_config.c            티커 정규화/파싱/직렬화 + 워치리스트 회전 (순수, 단위테스트 대상)
├── form_parse.c·json_build.c  폼 디코드 / JSON 이스케이프 (순수, 단위테스트 대상)
├── prov_wifi.c              STA(재시도+타임아웃, 접속 후 상시 재접속) + SoftAP + 스캔
├── prov_portal.c            캡티브 포털(HTTP + DNS 하이재킹) · portal.html
├── prov_store.c             NVS 저장/로드 (네임스페이스 prov)
├── provisioning.c           오케스트레이터: 저장된 망 접속 → 실패 시 포털 → 저장 → 재부팅
└── net_time.c               접속 후 1회 SNTP 시각 동기화 (뉴스 기간/차트 라벨용)

components/user_app/user_app.cpp  앱 글루: cJSON PSRAM 훅 + 워치리스트 순환 fetch 태스크
main/main.cpp                     부팅: 상태화면 → provisioning_run → net_time_sync → 주식 UI

sim/http_port_curl.c     [시뮬레이터 전용] 동일 http_get 을 libcurl 로 구현
```

> **부팅 흐름**: `app_main`이 NVS/디스플레이/LVGL을 올리고 설정 안내 화면을 띄운 뒤
> `provisioning_run()`을 호출한다. 저장된 WiFi가 있으면 접속(실패 시 캡티브 포털로 폴백),
> 접속되면 `net_time_sync()`로 시각을 맞추고 주식 UI를 띄운다. 이후 fetch 태스크가
> `STOCK_REFRESH_SECONDS`마다 워치리스트의 다음 티커로 전환하며 데이터를 갱신한다.

`http_get` 위의 모든 코드(파서·서비스·UI)는 펌웨어와 시뮬레이터가 **글자 그대로 동일**하게
컴파일한다. 덕분에 맥(인터넷 가능)에서 실데이터로 fetch→parse→render 전체 경로를
스크린샷으로 검증할 수 있다.

## 데이터 소스

| 화면 요소 | 소스 | 엔드포인트 |
|-----------|------|-----------|
| 현재가 / 증감 / 증감% | **Finnhub** `/quote` | 키 필요, 실시간성 양호 |
| 투자지표 (PER·EPS·시총·52주·배당) | **Finnhub** `/stock/metric?metric=all` | 키 필요 (~240KB → PSRAM) |
| 뉴스 헤드라인 (최근 7일) | **Finnhub** `/company-news` | 키 필요 |
| 당일 분봉 라인차트 | **Yahoo** v8 `/chart?range=1d&interval=5m` | 키 불필요 |

> Finnhub 무료 플랜은 `/stock/candle`(분봉)을 막아 두어, 분봉 차트만 Yahoo를 쓴다.
> Yahoo v8 chart는 키가 필요 없지만 IP별 레이트리밋이 있다(아래 한계 참고).

## 갱신 주기

- **화면 순환**: `CONFIG_STOCK_ROTATE_SECONDS`(기본 3초) 마다 차트→뉴스→지표 순환.
- **데이터 재요청**: `CONFIG_STOCK_REFRESH_SECONDS`(기본 30초) 마다 백그라운드 fetch.
  화면은 캐시를 그리므로 3초 순환은 매번 새로 받지 않는다(레이트리밋 보호).

## 설정

### 런타임 프로비저닝 (보드의 캡티브 포털)

WiFi 자격증명과 **워치리스트(티커 목록)** 는 부팅 시 보드가 띄우는 오픈 SoftAP
`Ticker Board-XXXX` 의 캡티브 포털(`http://192.168.4.1`)에서 입력해 NVS에 저장한다.
코드/Kconfig 수정 없이 사용자가 직접 설정하며, 비밀번호가 틀리면 다음 부팅 접속이 실패해
포털이 자동으로 다시 뜬다. 접속에 성공하면 앱이 입력한 워치리스트를 순환 표시한다.

### Kconfig (menuconfig → "Stock Monitor")

| Kconfig | 기본값 | 설명 |
|---------|--------|------|
| `STOCK_FINNHUB_API_KEY` | (빈값) | Finnhub 무료 키. **실제 키를 커밋하지 말 것** |
| `STOCK_SYMBOL` | `AAPL` | 워치리스트가 비었을 때만 쓰는 폴백 티커 |
| `STOCK_ROTATE_SECONDS` | 3 | 화면(차트/뉴스/지표) 순환 간격 |
| `STOCK_REFRESH_SECONDS` | 30 | 데이터 재요청 간격(이때 워치리스트의 다음 티커로 전환) |

Finnhub 키는 Kconfig(로컬 `sdkconfig`, gitignore됨)에만 저장한다. **WiFi 비밀번호는 더 이상
Kconfig가 아니라 포털→NVS 로만** 저장된다(저장소에는 어떤 자격증명도 커밋되지 않는다).

## 빌드 / 실행

### 시뮬레이터 (보드 없이, 맥에서 실데이터 검증)
```bash
cd sim
FINNHUB_KEY=<키> STOCK_SYMBOL=AAPL ./sim.sh
# → shots/sim_page0.png(차트) sim_page1.png(뉴스) sim_page2.png(지표)
```

### 호스트 단위 테스트 (파서)
```bash
cd components/stock_core/test/host
cmake -S . -B build && cmake --build build && ./build/test_parse
```

### 호스트 단위 테스트 (프로비저닝 순수 로직)
```bash
sh components/provisioning/test/run.sh   # 티커 정규화/회전·폼 디코드·JSON 이스케이프
```

### 펌웨어 (디바이스)
```bash
source "/Users/mimi/.espressif/tools/activate_idf_v6.0.1.sh"
cd <repo root>            # ESP-IDF 프로젝트 = 저장소 루트
idf.py menuconfig          # Stock Monitor 메뉴에서 키/SSID/심볼 설정
idf.py build
idf.py -p <PORT> flash monitor
```

## 검증 상태

- ✅ 파서: 호스트 단위테스트 통과(실제 Finnhub 응답 + Yahoo 스키마 픽스처).
- ✅ 데이터+UI: 시뮬레이터에서 **실데이터**로 3페이지 렌더링 스크린샷 확인.
- ✅ 펌웨어: `idf.py build` 컴파일 통과.
- ⛔ 디바이스 실물 동작(SPI 패널/WiFi/실측 명암)은 **보드에서만** 확인 가능 — 사용자 단계.

## 알아둘 점 / 한계

- **Yahoo 레이트리밋**: 개발 호스트(맥) IP가 차단되면 시뮬레이터는 차트만 합성 데이터로
  대체해 렌더러를 검증한다(로그에 `SYNTHETIC` 표기). 보드는 다른 IP라 보통 정상 동작한다.
  지속 429면 폴링 간격을 늘리거나 캐시를 활용한다.
- **흑백 폰트**: 내장 Montserrat에 없는 문자(곡선 따옴표·대시·…)는 파서에서 ASCII로
  치환해 tofu(□)를 방지한다(`to_ascii`). 비ASCII 종목/뉴스는 단순화되어 표시될 수 있다.
- **메모리**: Finnhub `metric=all`(~240KB)은 PSRAM에 누적하고 cJSON 트리도 PSRAM에 둔다
  (`cJSON_InitHooks`). 8MB PSRAM에서 여유 있음.
- **WiFi/프로비저닝**: `components/provisioning`이 STA 접속·SoftAP·캡티브 포털·NVS 저장을
  모두 담당한다(`net_time`의 SNTP 동기화 포함). 최초 접속 성공 후에는 끊겨도 무한 재접속한다.
  설정 화면의 네트워크 스캔은 단일 라디오 특성상 접속 중인 폰을 잠깐 끊을 수 있다
  (수동 ⟳ 재스캔 또는 "Other network…" 직접 입력으로 회복).
- **TLS 인증서(중요)**: finnhub.io는 Google Trust Services 체인을 **원본 "GlobalSign Root CA"(1998)** 로
  교차서명한다. ESP-IDF 풀 번들에는 GlobalSign R3/R6·GTS R1~R4만 있고 이 원본 루트가 없어서
  `No matching trusted root certificate found` 로 finnhub 호출이 전부 실패한다(야후=DigiCert는 정상).
  해결: `certs/globalsign_root_ca.pem` 를 `CONFIG_MBEDTLS_CUSTOM_CERTIFICATE_BUNDLE`(+PATH)로
  풀 번들에 추가했다(sdkconfig.defaults에 반영됨). 이 루트는 2028-01 만료 — 그 전에 finnhub가
  체인을 갱신할 가능성이 높고, 그때는 번들의 GTS R4로 충분하다.
```
