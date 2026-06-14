/*
 * wifi_conn.c — minimal WiFi station + SNTP, configured from Kconfig.
 */
#include "wifi_conn.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "wifi";
static EventGroupHandle_t s_eg;
#define BIT_GOT_IP BIT0

static void on_evt(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg; (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "disconnected, retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "got IP");
        xEventGroupSetBits(s_eg, BIT_GOT_IP);
    }
}

void wifi_conn_start(void) {
    s_eg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        on_evt, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        on_evt, NULL, NULL));
    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, CONFIG_STOCK_WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, CONFIG_STOCK_WIFI_PASSWORD, sizeof(wc.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "connecting to '%s'", CONFIG_STOCK_WIFI_SSID);
}

bool wifi_conn_wait(int timeout_ms) {
    if (!s_eg) return false;
    EventBits_t b = xEventGroupWaitBits(s_eg, BIT_GOT_IP, pdFALSE, pdTRUE,
                                        pdMS_TO_TICKS(timeout_ms));
    return (b & BIT_GOT_IP) != 0;
}

void wifi_conn_sync_time(int timeout_ms) {
    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    if (esp_netif_sntp_init(&cfg) != ESP_OK) {
        ESP_LOGW(TAG, "sntp init failed");
        return;
    }
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(timeout_ms)) != ESP_OK)
        ESP_LOGW(TAG, "sntp sync timeout (news dates may be off)");
    else
        ESP_LOGI(TAG, "time synced");
}
