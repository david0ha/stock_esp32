#include "prov_wifi.h"

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_log.h"

#define CONNECTED_BIT BIT0
#define FAIL_BIT      BIT1
#define MAX_RETRY     6

static const char *TAG = "prov_wifi";

static EventGroupHandle_t s_events;
static volatile bool s_connecting;
// Touched by both the Wi-Fi event task and the caller (prov_wifi_connect); volatile keeps
// reads/writes visible across them, matching s_connecting.
static volatile int s_retries;

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base != WIFI_EVENT) {
        return;
    }
    if (id == WIFI_EVENT_STA_DISCONNECTED && s_connecting) {
        if (s_retries < MAX_RETRY) {
            s_retries++;
            ESP_LOGI(TAG, "retry %d/%d", s_retries, MAX_RETRY);
            esp_wifi_connect();
        } else {
            xEventGroupSetBits(s_events, FAIL_BIT);
        }
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "got IP " IPSTR, IP2STR(&e->ip_info.ip));
        s_retries = 0;
        xEventGroupSetBits(s_events, CONNECTED_BIT);
    }
}

void prov_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // We persist credentials ourselves in NVS, so keep the driver's own copy in RAM only.
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_ip_event, NULL, NULL));

    s_events = xEventGroupCreate();

    // Start in station mode; configs are applied per-call below.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

bool prov_wifi_connect(const char *ssid, const char *password, uint32_t timeout_ms)
{
    wifi_config_t wc = {0};
    strlcpy((char *)wc.sta.ssid, ssid, sizeof(wc.sta.ssid));
    strlcpy((char *)wc.sta.password, password ? password : "", sizeof(wc.sta.password));
    // Accept any network from open upward (threshold is a *minimum* authmode).
    wc.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wc.sta.pmf_cfg.capable = true;
    wc.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));

    s_retries = 0;
    s_connecting = true;
    xEventGroupClearBits(s_events, CONNECTED_BIT | FAIL_BIT);

    ESP_LOGI(TAG, "connecting to '%s'", ssid);
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(
        s_events, CONNECTED_BIT | FAIL_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(timeout_ms));

    bool ok = (bits & CONNECTED_BIT) != 0;
    if (!ok) {
        s_connecting = false;
        esp_wifi_disconnect();
        ESP_LOGW(TAG, "connect to '%s' failed", ssid);
    }
    return ok;
}

void prov_wifi_start_ap(const char *ap_ssid)
{
    s_connecting = false;  // stop station reconnect attempts

    wifi_config_t wc = {0};
    strlcpy((char *)wc.ap.ssid, ap_ssid, sizeof(wc.ap.ssid));
    wc.ap.ssid_len = strlen(ap_ssid);
    wc.ap.channel = 1;
    wc.ap.max_connection = 4;
    wc.ap.authmode = WIFI_AUTH_OPEN;  // open network for easy join

    // APSTA so the station interface remains available for scanning.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wc));
    ESP_LOGI(TAG, "SoftAP '%s' up at 192.168.4.1", ap_ssid);
}

size_t prov_wifi_scan(prov_ap_t *out, size_t max)
{
    wifi_scan_config_t sc = {0};
    sc.show_hidden = false;
    sc.scan_type = WIFI_SCAN_TYPE_ACTIVE;

    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        return 0;
    }

    uint16_t found = 0;
    esp_wifi_scan_get_ap_num(&found);
    if (found == 0) {
        return 0;
    }

    wifi_ap_record_t *recs = calloc(found, sizeof(wifi_ap_record_t));
    if (recs == NULL) {
        esp_wifi_clear_ap_list();
        return 0;
    }

    uint16_t got = found;
    esp_wifi_scan_get_ap_records(&got, recs);

    size_t n = 0;
    for (uint16_t i = 0; i < got && n < max; i++) {
        if (recs[i].ssid[0] == '\0') {
            continue;  // hidden / empty SSID
        }
        strlcpy(out[n].ssid, (const char *)recs[i].ssid, sizeof(out[n].ssid));
        out[n].rssi = recs[i].rssi;
        out[n].secure = recs[i].authmode != WIFI_AUTH_OPEN;
        n++;
    }
    free(recs);
    return n;
}

void prov_wifi_mac_suffix(char *out)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    static const char hex[] = "0123456789ABCDEF";
    out[0] = hex[(mac[4] >> 4) & 0xF];
    out[1] = hex[mac[4] & 0xF];
    out[2] = hex[(mac[5] >> 4) & 0xF];
    out[3] = hex[mac[5] & 0xF];
    out[4] = '\0';
}
