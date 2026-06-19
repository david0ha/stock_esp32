// Pure (host-testable) configuration model for WiFi + ticker provisioning.
// This header MUST NOT depend on ESP-IDF so it can be unit-tested on the host.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define PROV_SSID_MAX_LEN     32   // 802.11 SSID limit
#define PROV_PASS_MAX_LEN     64   // WPA2 passphrase limit
#define PROV_TICKER_MAX_LEN   12   // per-symbol character cap
#define PROV_MAX_TICKERS      16   // how many symbols we keep
#define PROV_LOCATION_MAX_LEN 48   // free-text weather location ("Seoul", "Paris, FR")

typedef struct {
    char   ssid[PROV_SSID_MAX_LEN + 1];
    char   password[PROV_PASS_MAX_LEN + 1];
    char   tickers[PROV_MAX_TICKERS][PROV_TICKER_MAX_LEN + 1];
    size_t ticker_count;
    // Free-text place the user typed for weather. The device geocodes it to a
    // coordinate (Open-Meteo) once online — the portal/AP has no internet to do
    // so itself — and shows the resolved "City, CC" as confirmation. Empty -> no
    // weather widget.
    char   location[PROV_LOCATION_MAX_LEN + 1];
} prov_config_t;

#ifdef __cplusplus
extern "C" {
#endif

// Normalize a single raw ticker token into `out` (must hold PROV_TICKER_MAX_LEN+1 bytes).
// Trims surrounding whitespace, uppercases ASCII letters, and keeps only the symbol
// characters [A-Z0-9.-]; any other character makes the token invalid. Returns true and
// fills `out` when the result is a non-empty symbol that fits; otherwise returns false
// and sets out[0] = '\0'.
bool prov_ticker_normalize(const char *raw, char *out);

// Parse a delimited list of ticker tokens (separators: comma, space, tab, newline,
// carriage return) from `input` into cfg->tickers / cfg->ticker_count. Each token is
// normalized via prov_ticker_normalize; invalid/empty tokens are skipped, duplicates are
// dropped, and the list is capped at PROV_MAX_TICKERS. Resets cfg->ticker_count first.
// A NULL input is treated as empty. Returns the resulting ticker count.
size_t prov_tickers_parse(prov_config_t *cfg, const char *input);

// Serialize cfg->tickers into a single comma-separated string (e.g. "AAPL,TSLA") in `out`
// (NUL-terminated, never exceeding out_size). Returns the string length written (excluding
// the NUL). If the buffer is too small the output is truncated at a clean boundary.
size_t prov_tickers_serialize(const prov_config_t *cfg, char *out, size_t out_size);

// Return the watchlist symbol for rotation tick `index`, wrapping modulo the number of
// saved tickers so repeated calls cycle through the whole list. Returns NULL when the
// watchlist is empty, so callers can fall back to a configured default symbol. The returned
// pointer is owned by `cfg` and stays valid for its lifetime.
const char *prov_config_ticker_at(const prov_config_t *cfg, size_t index);

#ifdef __cplusplus
}
#endif
