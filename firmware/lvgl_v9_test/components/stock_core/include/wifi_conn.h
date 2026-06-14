/*
 * wifi_conn.h — minimal Kconfig-driven WiFi station + SNTP time sync.
 *
 * This is intentionally simple; WiFi provisioning is the part you said you'd
 * own. It connects to CONFIG_STOCK_WIFI_SSID/PASSWORD and keeps retrying.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void wifi_conn_start(void);              /* begin connecting (non-blocking)   */
bool wifi_conn_wait(int timeout_ms);     /* block until got IP, or timeout    */
void wifi_conn_sync_time(int timeout_ms);/* SNTP sync (needed for news dates) */

#ifdef __cplusplus
}
#endif
