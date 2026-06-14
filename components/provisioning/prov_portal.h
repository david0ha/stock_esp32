// Captive portal: HTTP config server + DNS hijack that serves the setup page and accepts
// the user's Wi-Fi credentials and ticker watchlist.
#pragma once

#include "prov_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Called when the user submits a valid configuration from the portal. `cfg` holds the
// parsed SSID/password/tickers and is ONLY valid for the duration of the call — it points at
// the HTTP handler's stack. Copy what you need (e.g. via prov_store_save); do not store the
// pointer or hand it to another task. Invoked from the HTTP server task.
typedef void (*prov_portal_save_cb_t)(const prov_config_t *cfg, void *user);

// Start the HTTP server (port 80) and DNS responder (port 53). `current` is shown on the
// page to pre-fill the saved SSID and tickers (pass NULL for none); `on_save` fires when a
// valid config is submitted.
void prov_portal_start(const prov_config_t *current, prov_portal_save_cb_t on_save, void *user);

#ifdef __cplusplus
}
#endif
