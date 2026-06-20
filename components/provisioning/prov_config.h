// Pure (host-testable) configuration model for WiFi + ticker provisioning.
// This header MUST NOT depend on ESP-IDF so it can be unit-tested on the host.
#pragma once

#include <stdbool.h>
#include <stddef.h>

#define PROV_SSID_MAX_LEN     32   // 802.11 SSID limit
#define PROV_PASS_MAX_LEN     64   // WPA2 passphrase limit
#define PROV_TICKER_MAX_LEN   12   // per-symbol character cap
#define PROV_MAX_TICKERS      16   // how many symbols we keep
#define PROV_FINNHUB_KEY_MAX  48   // Finnhub API key (real keys ~20 chars)
#define PROV_FMP_KEY_MAX      48   // FMP / econ-proxy key or shared secret
#define PROV_ECON_URL_MAX    127   // economic-calendar base URL (FMP or self-hosted proxy)

typedef struct {
    char   ssid[PROV_SSID_MAX_LEN + 1];
    char   password[PROV_PASS_MAX_LEN + 1];
    char   tickers[PROV_MAX_TICKERS][PROV_TICKER_MAX_LEN + 1];
    size_t ticker_count;
    // Runtime data-source config, entered in the companion app instead of menuconfig and saved to
    // NVS. Empty means "fall back to the compile-time Kconfig default" (so existing builds keep
    // working unchanged). econ_url empty -> econ_service's built-in default URL.
    char   finnhub_key[PROV_FINNHUB_KEY_MAX + 1];
    char   fmp_key[PROV_FMP_KEY_MAX + 1];
    char   econ_url[PROV_ECON_URL_MAX + 1];
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

// Result of prov_validate_credentials — mirrors the error codes the JSON API reports to the
// companion app (POST /api/provision). Kept here (pure) so the identical validation runs in
// the host tests and the firmware handler.
typedef enum {
    PROV_CRED_OK = 0,
    PROV_CRED_SSID_EMPTY,
    PROV_CRED_SSID_TOO_LONG,   // strlen(ssid) > PROV_SSID_MAX_LEN
    PROV_CRED_PASS_TOO_LONG,   // strlen(password) > PROV_PASS_MAX_LEN
} prov_cred_result_t;

// Validate submitted Wi-Fi credentials without storing them. NULL ssid/password is treated as
// empty. An empty password is allowed (open networks); only an empty SSID is rejected.
prov_cred_result_t prov_validate_credentials(const char *ssid, const char *password);

#ifdef __cplusplus
}
#endif
