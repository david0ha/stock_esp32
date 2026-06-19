/*
 * weather_service.h — builds the Open-Meteo URLs, fetches via the http_get port,
 * and parses into the weather model. Shared by firmware + simulator.
 *
 * Open-Meteo is free and needs no API key, so there is no secret here.
 */
#pragma once

#include "weather_model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Geocode a free-text place query (e.g. "Seoul" or "Paris, France") to its best
 * match. Returns 1 on success (out->valid = true), 0 on network/parse failure or
 * no match (out->valid = false). */
int weather_service_geocode(const char *query, geo_loc_t *out);

/* Fetch current conditions + a WX_FORECAST_MAX-day forecast for a coordinate.
 * Returns 1 on success (out->valid = true), 0 on network/parse failure. */
int weather_service_fetch(double lat, double lon, weather_t *out);

#ifdef __cplusplus
}
#endif
