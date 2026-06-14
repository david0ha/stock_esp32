#include "provisioning.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "prov_store.h"
#include "prov_wifi.h"
#include "prov_portal.h"

static const char *TAG = "provisioning";

static const prov_options_t *s_opts;

static void emit(prov_event_t event, const char *info)
{
    if (s_opts && s_opts->event_cb) {
        s_opts->event_cb(event, info, s_opts->user);
    }
}

static void reboot_task(void *arg)
{
    (void)arg;
    // Give the HTTP response time to flush to the browser before restarting.
    vTaskDelay(pdMS_TO_TICKS(1500));
    ESP_LOGI(TAG, "restarting to apply new configuration");
    esp_restart();
}

static void on_portal_save(const prov_config_t *cfg, void *user)
{
    (void)user;
    if (!prov_store_save(cfg)) {
        // Persisting failed: do NOT reboot (that would just relaunch the empty
        // portal). Stay up so the user can retry the submission.
        ESP_LOGE(TAG, "save failed — staying in setup portal for retry");
        return;
    }
    emit(PROV_EVENT_CONFIG_SAVED, cfg->ssid);
    xTaskCreate(reboot_task, "prov_reboot", 2048, NULL, 5, NULL);
}

static void ensure_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void provisioning_default_options(prov_options_t *opts)
{
    memset(opts, 0, sizeof(*opts));
    strlcpy(opts->ap_ssid_prefix, "Ticker Board", sizeof(opts->ap_ssid_prefix));
    opts->sta_connect_timeout_ms = 15000;
    opts->event_cb = NULL;
    opts->user = NULL;
}

bool provisioning_run(const prov_options_t *opts, prov_config_t *out)
{
    s_opts = opts;

    ensure_nvs();

    prov_config_t cfg;
    bool have_config = prov_store_load(&cfg);

    prov_wifi_init();

    if (have_config) {
        ESP_LOGI(TAG, "stored network '%s' — attempting to connect", cfg.ssid);
        emit(PROV_EVENT_STA_CONNECTING, cfg.ssid);
        if (prov_wifi_connect(cfg.ssid, cfg.password, opts->sta_connect_timeout_ms)) {
            emit(PROV_EVENT_STA_CONNECTED, cfg.ssid);
            *out = cfg;
            return true;
        }
        ESP_LOGW(TAG, "could not join '%s' — falling back to setup portal", cfg.ssid);
    } else {
        ESP_LOGI(TAG, "no stored network — starting setup portal");
    }

    char suffix[5];
    prov_wifi_mac_suffix(suffix);
    char ap_ssid[40];
    snprintf(ap_ssid, sizeof(ap_ssid), "%s-%s", opts->ap_ssid_prefix, suffix);

    // Scan now, while still STA-only with no client attached. Once the SoftAP is up and a
    // phone is connected, a live scan would leave the AP channel and reset that connection,
    // so the portal serves these cached results instead.
    prov_wifi_cache_scan();

    prov_wifi_start_ap(ap_ssid);
    prov_portal_start(have_config ? &cfg : NULL, on_portal_save, NULL);
    emit(PROV_EVENT_PORTAL_STARTED, ap_ssid);

    ESP_LOGI(TAG, "setup portal ready — join Wi-Fi '%s' and open http://192.168.4.1", ap_ssid);

    // Stay in setup mode; on_portal_save persists the config and reboots into station mode.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
