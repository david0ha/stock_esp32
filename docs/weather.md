# Weather Widget (Open-Meteo, free, no API key needed)

The home dashboard's current weather (icon + °C + city name) and 7-day forecast strip are
fetched from [Open-Meteo](https://open-meteo.com). **No API key is needed** —
it is free and unlimited for non-commercial use, so there is no secret to share.

## How It Works

1. In the **captive portal**, you enter a **location name as text** rather than latitude/longitude
   (e.g. `Seoul`, `Paris`, `Austin, US`). During the portal SoftAP phase there is no internet, so
   geocoding cannot be done right there; the input value is stored as-is in NVS (`prov` namespace,
   key `loc`).
2. After WiFi connects, `WeatherTask` (`components/user_app/user_app.cpp`) converts that text into
   coordinates via the Open-Meteo **geocoding API** (the first, nearest result).
   The resolved `"city, country code"` (e.g. `Seoul, KR`) is shown on screen so you can **confirm
   the address was resolved correctly**.
3. From then on, it refreshes the **current weather + 7-day forecast** every 30 minutes using the coordinates.

If you leave the location blank in the portal, the weather widget is not displayed.

## Code Structure (Portable Core)

| File | Role |
|------|------|
| `components/stock_core/include/weather_model.h` | `geo_loc_t` / `weather_t` / 4-state `wx_kind_t` (clear, partly cloudy, overcast, rain — what the panel can draw) |
| `components/stock_core/weather_parse.c` | Geocoding/forecast JSON parser + WMO code→glyph mapping + day-of-week computation (subject to host tests) |
| `components/stock_core/weather_service.c` | Keyless Open-Meteo URL assembly + `http_get` calls |
| `components/stock_core/test/host/test_weather.c` | Parser unit tests (`om_geo.json`, `om_forecast.json` fixtures) |

The `wx_kind_t` values are in the same order as `home_wx_t` in `ui_home.h`, so the UI bridge is a simple cast
(enforced by a `static_assert` in `user_app.cpp`).

## Verifying Real Data with the Simulator

Since no key is needed, you can render live weather directly in the desktop simulator:

```bash
cd sim
LOCATION="Seoul" ./sim.sh        # render the home page with real Open-Meteo data
```

If `LOCATION` is not set, it falls back to the reference sample weather.

## Keys/Secrets for Other Features (Reference)

Unlike weather, the two below require keys and are not committed to the repository:

- **Stock prices (Finnhub)** — `idf.py menuconfig → Stock Monitor → Finnhub API key`
  (`CONFIG_STOCK_FINNHUB_API_KEY`, stored only in the local `sdkconfig`). Free key:
  https://finnhub.io . Used for real-time price/change of the 3 sidebar tickers.
- **Economic calendar** — set the `ECON_PROXY_TOKEN` of `tools/econ_proxy` (`.env`, gitignored) to the same
  value as the device's `CONFIG_STOCK_FMP_API_KEY`. For details see
  [econ-proxy-deployment.md](econ-proxy-deployment.md).
