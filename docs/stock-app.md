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
├── wifi_conn.c              [펌웨어 전용] Kconfig 기반 WiFi STA + SNTP
└── Kconfig                  사용자 설정(아래)

sim/http_port_curl.c     [시뮬레이터 전용] 동일 http_get 을 libcurl 로 구현
```

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

## 설정 (menuconfig → "Stock Monitor")

| Kconfig | 기본값 | 설명 |
|---------|--------|------|
| `STOCK_SYMBOL` | `AAPL` | 표시할 티커 |
| `STOCK_FINNHUB_API_KEY` | (빈값) | Finnhub 무료 키. **실제 키를 커밋하지 말 것** |
| `STOCK_WIFI_SSID` / `STOCK_WIFI_PASSWORD` | (빈값) | 2.4GHz STA 접속 정보 |
| `STOCK_ROTATE_SECONDS` | 3 | 화면 순환 간격 |
| `STOCK_REFRESH_SECONDS` | 30 | 데이터 재요청 간격 |

키는 Kconfig(로컬 `sdkconfig`)에만 저장된다. 저장소의 `sdkconfig` 기본 키는 빈 문자열이다.

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

### 펌웨어 (디바이스)
```bash
source "~/.espressif/tools/activate_idf_v6.0.1.sh"
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
- **WiFi**: 최소 구현(Kconfig STA)이다. 프로비저닝/재접속 정책은 직접 확장하면 된다.
- **TLS 인증서(중요)**: finnhub.io는 Google Trust Services 체인을 **원본 "GlobalSign Root CA"(1998)** 로
  교차서명한다. ESP-IDF 풀 번들에는 GlobalSign R3/R6·GTS R1~R4만 있고 이 원본 루트가 없어서
  `No matching trusted root certificate found` 로 finnhub 호출이 전부 실패한다(야후=DigiCert는 정상).
  해결: `certs/globalsign_root_ca.pem` 를 `CONFIG_MBEDTLS_CUSTOM_CERTIFICATE_BUNDLE`(+PATH)로
  풀 번들에 추가했다(sdkconfig.defaults에 반영됨). 이 루트는 2028-01 만료 — 그 전에 finnhub가
  체인을 갱신할 가능성이 높고, 그때는 번들의 GTS R4로 충분하다.
```
