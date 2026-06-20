# 컴패니언 앱 + ESP32 제어 API (App Control)

이 문서는 **로컬 전용**(클라우드 없음) React Native 컴패니언 앱과 펌웨어 사이의
HTTP/JSON 계약을 정의한다. 앱은 두 단계로 보드와 통신한다.

```
[1] 온보딩 (SoftAP)           [2] 제어 (집 LAN, STA)
 phone ── join AP ──► ESP32     phone ──┐
   http://192.168.4.1            (same Wi-Fi)
   /api/info  /api/scan          └─► http://tickerboard.local  (mDNS)  또는  http://<device-ip>
   /api/provision /api/status        /api/info  /api/stock/*  /api/econ
```

- 단일 라디오 보드라 온보딩이 끝나면 보드는 SoftAP 를 내리고 STA(집 와이파이)로 재부팅한다.
  그 이후의 "실시간 제어"는 집 LAN 위에서 이뤄진다(mDNS `tickerboard.local`, 폴백은 수동 IP).
- 모든 자격증명/워치리스트는 NVS 에만 저장되고 저장소에는 커밋되지 않는다.

## 디자인 출처 (참고 레포)

- `~/Documents/masterham-esp32` — 동일 계보의 `provisioning` 컴포넌트가 이미 `/api/*` 프로비저닝
  JSON API + 호스트 테스트되는 `prov_json.c` 를 갖고 있다 → **그대로 이식**한다.
- `~/Documents/masterham-server/app` — Expo **Dev Client**(Expo Go 아님) RN 앱이 ESP32 를 AP 로
  온보딩한다. `src/lib/esp32.ts`(테스트됨)·`scripts/mock-esp32.js` 패턴을 차용하되 클라우드
  (Supabase/AWS/MQTT)는 전부 제거하고 로컬 HTTP 만 남긴다.

---

## [1] 프로비저닝 API (SoftAP, `http://192.168.4.1`)

masterham 과 1:1 동일. 펌웨어: `components/provisioning/prov_portal.c`.

| 메서드/경로 | 요청 | 응답 |
|---|---|---|
| `GET /api/info` | — | `{ "deviceId", "model", "apSsid" }` |
| `GET /api/scan` | — | `{ "networks":[{"ssid","rssi","secure"}] }` (캐시 스캔) |
| `POST /api/provision` | `x-www-form-urlencoded`: `ssid`,`ssid_manual`,`password`,`tickers`,`finnhub_key`,`fmp_key`,`econ_url`(뒤 3개 선택) | `202 {"ok":true,"state":"connecting"}` / `4xx {"ok":false,"error":<code>}` |
| `GET /api/status` | — | `{ "state":"idle\|connecting\|connected\|failed", "ssid"?, "reason"? }` |

- `provision` 에러 코드: `ssid_empty` `ssid_too_long` `pass_too_long` `too_large` `read_error`.
  (96바이트 이상으로 디코드되는 병적 길이 비밀번호는 URL 디코더가 빈 값으로 처리해 `pass_too_long`
  대신 열린 네트워크로 간주될 수 있다 — 실제 WPA2 한도(63자)를 한참 넘는 입력에서만 발생하는 알려진 한계.)
- `provision` 은 비동기 connect-test 를 시작하고 **즉시 202** 를 반환한다. 앱은 `/api/status`
  를 폴링한다(`waitForConnected`). connect-test 는 SoftAP 를 유지한 채(`prov_wifi_connect_keep_ap`)
  자격증명을 검증하고, 성공 시 NVS 저장 후 재부팅 → STA 모드로 올라온다.
- ⚠️ **단일 라디오 한계(중요)**: ESP32 는 라디오가 1개라 APSTA 에서 SoftAP 채널이 STA 채널을 따라간다.
  따라서 join 성공 직후 SoftAP 가 대상 AP 채널로 hop 하면 채널 1에 머문 폰이 192.168.4.1 에 닿지 못해
  `connected` 를 못 읽고 AP 가 끊길 수 있다. 그래서 앱은 **202 수신 후 AP 가 끊기거나 status 폴링이
  타임아웃하면 "연결됨(추정)"으로 간주**하고, 홈 Wi-Fi 재접속 → mDNS(`tickerboard.local`)로 기기를
  재발견해 최종 확인한다. 반면 **실패(잘못된 비밀번호)** 시에는 SoftAP 가 채널 1로 복귀하므로 폰이
  `failed` 상태를 정상적으로 읽는다(실패 감지는 신뢰 가능, 성공 확인만 LAN 재발견에 의존).
- `tickers` 는 기존 HTML 폼과 동일하게 콤마/공백 구분 워치리스트. 온보딩 한 번에 Wi-Fi +
  워치리스트를 모두 설정한다.
- **API 키/URL 은 menuconfig 가 아니라 앱에서 입력**해 NVS 에 저장한다(빈 값이면 Kconfig 기본값으로
  폴백): `finnhub_key`(주가), `fmp_key`(경제 캘린더 키/프록시 토큰), `econ_url`(경제 캘린더 base URL,
  FMP 직결 또는 자체 호스팅 프록시). 온보딩 후에도 아래 `POST /api/stock/keys` 로 변경 가능.
- **캡티브 포털 자동 팝업은 제거**됐다(DNS 하이재킹 + OS 프로브 302 리다이렉트 삭제). AP 에 접속해도
  "Wi-Fi 로그인" 시트가 뜨지 않으며, 앱이 고정 IP `192.168.4.1` 의 JSON API 로 프로비저닝한다(브라우저
  설정 페이지는 `http://192.168.4.1/` 로 직접 접속 시 폴백으로 여전히 사용 가능).

## [2] 스톡 제어 API (STA, `http://tickerboard.local` 또는 IP)

펌웨어: 신규 `components/stock_api`(디바이스 전용 httpd + mDNS) + `components/stock_core`
의 순수 직렬화기 `stock_api_json.c`. `user_app` 의 스레드 안전 제어 브리지를 호출한다.

### `GET /api/info`
온보딩과 동일 스키마(STA 식별/발견용). `apSsid` 는 빈 문자열일 수 있다.
```json
{ "deviceId":"9F3A", "model":"Ticker Board", "fw":"0.1.0", "ip":"192.168.0.42" }
```

### `GET /api/stock/state` — 라이브 스냅샷(앱 대시보드가 폴링)
```json
{
  "model":"Ticker Board", "fw":"0.1.0", "deviceId":"9F3A", "ip":"192.168.0.42",
  "index": 0,                       // 화면에 떠 있는 워치리스트 인덱스
  "page": 0,                        // 0=home 1=chart 2=news 3=metrics
  "econMode": false, "econWeek": 0, // 경제 캘린더 오버레이 상태
  "refreshSeconds": 30,
  "keys": { "finnhub": true, "fmp": false, "econUrl": true },   // 설정 여부만(값은 노출 안 함)
  "env": { "valid": true, "tempC": 24.3, "humidity": 41.0,
           "batteryValid": true, "batteryV": 4.02, "batteryPct": 88 },
  "watchlist": [
    { "symbol":"AAPL", "valid":true,  "price":201.5, "change":1.2, "percent":0.6, "ageSec":12 },
    { "symbol":"TSLA", "valid":false, "price":0, "change":0, "percent":0, "ageSec":-1 }
  ]
}
```
- `valid=false` / `ageSec=-1` → 아직 한 번도 못 받은 슬롯(앱은 "loading" 표시).
- 숫자는 `double`. `ageSec` 는 마지막 fetch 이후 경과 초(미수신은 -1).

### 제어 (모두 `application/json` 바디, 즉시 `200 {"ok":true}` 또는 `4xx {"ok":false,"error":...}`)
| 경로 | 바디 | 동작 |
|---|---|---|
| `POST /api/stock/select` | `{"index":2}` 또는 `{"symbol":"TSLA"}` | 화면 티커 전환(버튼 USER 와 동일). symbol 은 펌웨어가 인덱스로 해석 |
| `POST /api/stock/page` | `{"page":1}` | 뷰 전환(버튼 BOOT 와 동일). 0..3 |
| `POST /api/stock/econ` | `{"mode":true,"week":0}` 또는 `{"mode":false}` | 경제 캘린더 오버레이 토글/주차 이동 |
| `POST /api/stock/refresh` | `{"all":false}` | 현재(또는 전체) 티커 강제 재요청 |
| `POST /api/stock/watchlist` | `{"tickers":["AAPL","TSLA",...]}` 또는 `{"tickers":"AAPL,TSLA"}` | 워치리스트 교체. **NVS 저장 + 즉시 적용(재부팅 없음)**. 1..16개, 정규화는 `prov_tickers_parse` 와 동일 |
| `POST /api/stock/keys` | `{"finnhubKey"?,"fmpKey"?,"econUrl"?}` | API 키/URL 라이브 변경. 존재하는 필드만 갱신(빈 문자열=초기화→Kconfig 폴백), NVS 저장 + 즉시 재요청. 값은 반환하지 않음(state 의 `keys` 는 설정 여부만) |

- 에러 코드: `bad_json` `index_range` `page_range` `symbol_not_found` `empty_watchlist` `too_many_tickers`.
  추가로 모든 `/api/stock/*` POST 는 본문 리더 공통 에러 `too_large`(본문 초과)·`read_error`(소켓 오류)도
  반환할 수 있다(프로비저닝 `/api/provision` 과 동일). 워치리스트 토큰 수는 배열·문자열 두 형식 모두
  16개 초과 시 `too_many_tickers`. 정규화 불가 심볼은 조용히 무시(드롭)된다.
- 쓰기 명령은 `user_app` 의 명령 큐로 들어가 `StockTask`(LVGL/UI 컨텍스트)에서 적용된다 →
  버튼과 동일한 코드 경로(`render_current`/`request_fetch`)라 경쟁이 없다.

### `GET /api/econ` — (선택) 현재 주차 경제 캘린더 이벤트 JSON
앱이 econ 오버레이를 미러링할 때 사용. 미구현 시 404 허용.

---

## 펌웨어 변경 요약

- `components/provisioning`
  - 신규 순수파일 **`prov_json.c/.h`**(masterham 이식) — 호스트 테스트(`test_prov_json.c`).
  - `prov_config.c/.h`: 순수 **`prov_validate_credentials`**(+ `prov_cred_result_t`) 추가.
  - `prov_portal.c/.h`: `/api/*` 핸들러 + `prov_portal_info_t` + `prov_portal_set_status` +
    비동기 provision 콜백(HTML `/save` 경로는 그대로 유지).
  - `prov_wifi.c/.h`: **`prov_wifi_connect_keep_ap`**(SoftAP 유지 검증) 추가.
  - `provisioning.c`: provision connect-test 태스크 + 상태 머신.
- `components/stock_core`
  - 신규 순수파일 **`stock_api_model.h`**(스냅샷 구조체) + **`stock_api_json.c/.h`**(직렬화기) +
    호스트 테스트.
- `components/stock_api` (신규, 디바이스 전용)
  - STA 모드 httpd(`/api/info`,`/api/stock/*`,`/api/econ`) + mDNS(`tickerboard`).
  - `user_app` 제어 브리지(`user_app_control.h`) 호출.
- `components/user_app`
  - `user_app_control.h`: `user_app_snapshot/select_index/set_page/set_econ/refresh/set_watchlist`.
  - `user_app.cpp`: 명령 큐 + 큐셋(버튼+명령), 스냅샷 수집, **s_cache 를 PROV_MAX_TICKERS 로
    선할당**(워치리스트 라이브 교체용), 워치리스트 적용 + NVS 저장.
- `main/main.cpp`: STA 접속 후 `stock_api_start()` 기동.
- 의존성: 매니지드 컴포넌트 `espressif/mdns` 추가.

## 앱 변경 요약 (`app/`)

- Expo **Dev Client** + expo-router + expo-build-properties(`usesCleartextTraffic`,
  `NSAllowsLocalNetworking`). **Expo Go 불가** → `npx expo run:ios|android` 네이티브 빌드.
- `src/lib/esp32.ts` — 프로비저닝 클라이언트(이식) + 스톡 제어 클라이언트(확장) + 타입. 단위테스트.
- `src/lib/discovery.ts` — base URL 결정(저장된 IP / `tickerboard.local` / 수동 입력).
- 화면: 온보딩(AP 안내 → Wi-Fi 스캔/선택 → 비번+워치리스트 → 상태 폴링) +
  제어 대시보드(연결 → 라이브 상태 → 워치리스트 편집 → 티커/뷰/econ 제어 → refresh).
- `scripts/mock-esp32.js` — 두 API 를 모두 모킹(하드웨어 없이 개발).

## 검증 경계(정직하게)

- ✅ 가능: 순수 로직 호스트 테스트(UBSan, ASan 금지 — 샌드박스에서 hang), `idf.py build`,
  앱 `tsc --noEmit` + `jest`, `node scripts/mock-esp32.js` 로 앱 흐름.
- ⛔ 불가(사용자 단계): 실제 보드 SPI 패널/Wi-Fi 라이브 동작, iOS/Android 실기기 네이티브 빌드.
