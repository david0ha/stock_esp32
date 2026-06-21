/*
 * econ_service.h — builds the FMP endpoint URL, fetches via the http_get port,
 * and parses into econ_calendar_t. Shared by firmware and simulator.
 */
#pragma once

#include "econ_model.h"
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fetch the FMP economics calendar for the Monday..Sunday week at `week_offset`
 * (0 = current, -1 = previous, +1 = next), relative to now_utc in the device
 * timezone (tz_off seconds east of UTC). Keeps events with impact >= min_impact.
 *
 * `base_url` is the economic-calendar endpoint base (FMP or a self-hosted proxy); NULL or empty
 * falls back to the compile-time CONFIG_STOCK_ECON_BASE_URL default.
 *
 * out->week_label is always set (so the error screen still names the week). On
 * any failure out->valid is false and out->error holds a human-readable reason
 * (missing key / HTTP status / transport failure / FMP "Error Message").
 * Returns 1 on success, 0 on failure. */
int econ_service_fetch(const char *fmp_key, const char *base_url, time_t now_utc, long tz_off,
                       int week_offset, int min_impact, econ_calendar_t *out);

#ifdef __cplusplus
}
#endif
