/*
 * stock_service.h — fetch + parse orchestration.
 *
 * Builds the Finnhub / Yahoo URLs, calls the http_get port, and runs the
 * parsers into `out`. Each sub-struct keeps its own `valid` flag so one dead
 * endpoint degrades gracefully instead of blanking the screen.
 *
 * Returns the number of endpoints that succeeded (0..4).
 */
#pragma once

#include "stock_model.h"

#ifdef __cplusplus
extern "C" {
#endif

int stock_service_fetch(const char *symbol, const char *finnhub_key,
                        stock_data_t *out);

#ifdef __cplusplus
}
#endif
