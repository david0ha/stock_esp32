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

// Bring up an open SoftAP named `ap_ssid` (mode becomes APSTA so scanning still works).
void prov_wifi_start_ap(const char *ap_ssid);

// Blocking scan of nearby networks; writes up to `max` entries into `out`, returns the count.
size_t prov_wifi_scan(prov_ap_t *out, size_t max);

// Write the 4-hex-digit MAC suffix (e.g. "9F3A") into `out` (needs >= 5 bytes).
void prov_wifi_mac_suffix(char *out);

#ifdef __cplusplus
}
#endif
