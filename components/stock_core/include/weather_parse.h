/*
 * weather_parse.h — pure JSON parsers for the Open-Meteo feeds (host-testable,
 * no board or network dependency). Both take a response body and fill a struct.
 */
#pragma once

#include "weather_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parse an Open-Meteo geocoding response
 *   GET https://geocoding-api.open-meteo.com/v1/search?name=<q>&count=N
 * into `out`, taking the FIRST result as the best match. On success returns 0
 * and sets out->valid = true (name/country/lat/lon filled). Returns -1 on
 * malformed JSON or an empty/absent "results" array (out->valid = false). */
int geo_parse(const char *json, geo_loc_t *out);

/* Parse an Open-Meteo forecast response
 *   GET .../v1/forecast?...&current=temperature_2m,weather_code
 *                          &daily=weather_code,temperature_2m_max,temperature_2m_min
 * into `out`: the "current" block -> now_*, and up to WX_FORECAST_MAX "daily"
 * entries -> days[] (weekday derived from each date). Returns 0 when at least
 * the current OR the daily block parsed (out->valid = true); -1 on malformed
 * JSON or when neither block is usable (out->valid = false). */
int weather_parse(const char *json, weather_t *out);

#ifdef __cplusplus
}
#endif
