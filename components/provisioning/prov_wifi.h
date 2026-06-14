// Thin Wi-Fi layer for provisioning: station connect with bounded retry, SoftAP, and scan.
// Built on ESP-IDF v6 esp_wifi/esp_netif. Hardware-coupled; verified by build + on-device.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char    ssid[33];
    int8_t  rssi;
    bool    secure;  // true unless the network is open (no password)
} prov_ap_t;

// One-time init: TCP/IP stack, default event loop, STA+AP netifs, Wi-Fi driver, handlers.
void prov_wifi_init(void);

// Try to join `ssid`/`password` as a station, waiting up to `timeout_ms` for an IP.
// Returns true on success. Safe to call after a previous failed attempt.
bool prov_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms);

// Bring up an open SoftAP named `ap_ssid` (mode becomes APSTA).
void prov_wifi_start_ap(const char *ap_ssid);

// Blocking scan of nearby networks; writes up to `max` entries into `out`, returns the count.
size_t prov_wifi_scan(prov_ap_t *out, size_t max);

// Refresh the cached scan results by scanning now. Call this BEFORE bringing up the SoftAP
// (while no client is connected): a scan briefly leaves the AP channel, which would reset a
// connected setup client's TCP session — so the captive portal must serve cached results, not
// scan live.
void prov_wifi_cache_scan(void);

// Copy up to `max` cached entries (from the last prov_wifi_cache_scan) into `out`; returns the
// count. Non-disruptive — safe to call while serving the captive portal.
size_t prov_wifi_scan_cached(prov_ap_t *out, size_t max);

// Write the 4-hex-digit MAC suffix (e.g. "9F3A") into `out` (needs >= 5 bytes).
void prov_wifi_mac_suffix(char *out);

#ifdef __cplusplus
}
#endif
