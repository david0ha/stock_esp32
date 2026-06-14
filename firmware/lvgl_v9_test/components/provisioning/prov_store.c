#include "prov_store.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define PROV_NVS_NS "prov"
// Worst case: PROV_MAX_TICKERS symbols of PROV_TICKER_MAX_LEN chars + separators + NUL.
#define TICKERS_CSV_MAX (PROV_MAX_TICKERS * (PROV_TICKER_MAX_LEN + 1) + 1)

static const char *TAG = "prov_store";

bool prov_store_load(prov_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    nvs_handle_t h;
    if (nvs_open(PROV_NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        return false;  // namespace not created yet -> nothing saved
    }

    size_t len = sizeof(cfg->ssid);
    nvs_get_str(h, "ssid", cfg->ssid, &len);

    len = sizeof(cfg->password);
    nvs_get_str(h, "pass", cfg->password, &len);

    char csv[TICKERS_CSV_MAX];
    len = sizeof(csv);
    if (nvs_get_str(h, "tickers", csv, &len) == ESP_OK) {
        prov_tickers_parse(cfg, csv);
    }

    nvs_close(h);
    return cfg->ssid[0] != '\0';
}

void prov_store_save(const prov_config_t *cfg)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(PROV_NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return;
    }

    char csv[TICKERS_CSV_MAX];
    prov_tickers_serialize(cfg, csv, sizeof(csv));

    nvs_set_str(h, "ssid", cfg->ssid);
    nvs_set_str(h, "pass", cfg->password);
    nvs_set_str(h, "tickers", csv);

    err = nvs_commit(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    }
    nvs_close(h);
    ESP_LOGI(TAG, "saved ssid='%s' tickers='%s'", cfg->ssid, csv);
}

void prov_store_clear(void)
{
    nvs_handle_t h;
    if (nvs_open(PROV_NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
}
