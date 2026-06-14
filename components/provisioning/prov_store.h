// NVS-backed persistence for the provisioning config (Wi-Fi credentials + tickers).
#pragma once

#include <stdbool.h>

#include "prov_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Load the saved config into `cfg` (zeroed first). Returns true if a network SSID is stored.
bool prov_store_load(prov_config_t *cfg);

// Persist `cfg` (SSID, password, and the tickers as a comma-separated string).
// Returns true if the write committed; false if opening or committing NVS failed.
bool prov_store_save(const prov_config_t *cfg);

// Erase all saved provisioning data.
void prov_store_clear(void);

#ifdef __cplusplus
}
#endif
