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

/*
 * Refresh ONLY the realtime quote (price / change / %) into out->quote, leaving
 * the heavier series / metrics / news already in `out` untouched. One small
 * Finnhub request — used for cheap periodic refreshes and ticker switches so we
 * don't re-download the ~240KB metric=all payload every time. The caller passes
 * the existing cached stock_data_t (do NOT zero it first). Returns 1 on
 * success, 0 on failure.
 */
int stock_service_fetch_quote(const char *symbol, const char *finnhub_key,
                              stock_data_t *out);

#ifdef __cplusplus
}
#endif
