# 날씨 위젯 (Open-Meteo, 무료·키 불필요)

홈 대시보드의 현재 날씨(아이콘 + °C + 도시명)와 7일 예보 스트립은
[Open-Meteo](https://open-meteo.com) 에서 가져온다. **API 키가 필요 없다** —
무료·비상업 무제한이라 공유할 시크릿이 없다.

## 동작 방식

1. **캡티브 포털**에서 위/경도가 아니라 **지역명을 텍스트로** 입력한다
   (예: `Seoul`, `Paris`, `Austin, US`). 포털 SoftAP 단계에는 인터넷이 없어
   여기서 바로 지오코딩을 할 수 없으므로, 입력값은 그대로 NVS(`prov` 네임스페이스,
   키 `loc`)에 저장된다.
2. WiFi 연결 후 `WeatherTask`(`components/user_app/user_app.cpp`)가
   Open-Meteo **지오코딩 API**로 그 텍스트를 좌표로 변환한다(가장 가까운 첫 결과).
   해석된 `"도시, 국가코드"`(예: `Seoul, KR`)가 화면에 표시되어 **주소가 제대로
   잡혔는지 확인**할 수 있다.
3. 이후 좌표로 **현재 날씨 + 7일 예보**를 30분마다 갱신한다.

포털에 위치를 비워두면 날씨 위젯은 표시되지 않는다.

## 코드 구성 (이식성 코어)

| 파일 | 역할 |
|------|------|
| `components/stock_core/include/weather_model.h` | `geo_loc_t` / `weather_t` / 4-state `wx_kind_t`(패널이 그릴 수 있는 맑음·구름조금·흐림·비) |
| `components/stock_core/weather_parse.c` | 지오코딩·예보 JSON 파서 + WMO 코드→글리프 매핑 + 요일 계산 (호스트 테스트 대상) |
| `components/stock_core/weather_service.c` | 키 없는 Open-Meteo URL 조립 + `http_get` 호출 |
| `components/stock_core/test/host/test_weather.c` | 파서 단위 테스트 (`om_geo.json`, `om_forecast.json` 픽스처) |

`wx_kind_t` 값은 `ui_home.h`의 `home_wx_t` 와 순서가 동일해 UI 브리지는 단순 캐스트다
(`user_app.cpp`의 `static_assert`가 이를 강제).

## 시뮬레이터로 실제 데이터 확인

키가 필요 없으므로 데스크톱 시뮬레이터에서 바로 라이브 날씨를 렌더할 수 있다:

```bash
cd sim
LOCATION="Seoul" ./sim.sh        # 실제 Open-Meteo 데이터로 홈 페이지 렌더
```

`LOCATION` 미설정 시 레퍼런스 샘플 날씨로 폴백한다.

## 다른 기능의 키/시크릿 (참고)

날씨와 달리 아래 둘은 키가 필요하며, 저장소에 커밋하지 않는다:

- **주가(Finnhub)** — `idf.py menuconfig → Stock Monitor → Finnhub API key`
  (`CONFIG_STOCK_FINNHUB_API_KEY`, 로컬 `sdkconfig` 에만 저장). 무료 키:
  https://finnhub.io . 사이드바 3종목 실시간 가격/변동에 사용.
- **경제 캘린더** — `tools/econ_proxy` 의 `ECON_PROXY_TOKEN`(`.env`, gitignore됨)을
  기기의 `CONFIG_STOCK_FMP_API_KEY` 와 동일하게 설정. 자세한 내용은
  [econ-proxy-deployment.md](econ-proxy-deployment.md).
