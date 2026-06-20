/*
 * stock_api_json.h — pure (host-testable) JSON serializers for the stock control API
 * (GET /api/info and GET /api/stock/state). No ESP-IDF deps, mirroring prov_json.h.
 *
 * Every builder NUL-terminates `out` and returns the written length (excluding the NUL), or -1
 * if the result would not fit in `out_size` (in which case `out` is set to "").
 */
#pragma once

#include <stddef.h>

#include "stock_api_model.h"

#ifdef __cplusplus
extern "C" {
#endif

// Build {"deviceId":"...","model":"...","fw":"...","ip":"..."}. NULL fields are emitted as "".
int stock_api_json_info(char *out, size_t out_size,
                        const char *device_id, const char *model,
                        const char *fw, const char *ip);

// Build the full GET /api/stock/state document from `st`. Watchlist entries past
// STOCK_API_MAX_TICKERS or that would overflow `out` are dropped (the array stays valid JSON);
// returns -1 only if even the envelope (without any tickers) does not fit.
int stock_api_json_state(const stock_api_state_t *st, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
