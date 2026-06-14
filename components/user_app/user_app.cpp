/*
 * user_app.cpp — app orchestration for the stock monitor.
 *
 *   AppInit:  NVS, route cJSON allocations to PSRAM, start WiFi.
 *   UiInit:   build the LVGL stock UI on a fresh screen.
 *   TaskInit: spawn the task that fetches data and rotates the 3 pages.
 *
 * The portable core (parsers, service, UI) lives in the stock_core component
 * and is the same code verified by the host tests and the desktop simulator.
 */
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <nvs_flash.h>

#include "sdkconfig.h"
#include "cJSON.h"

#include "user_app.h"
#include "lvgl_bsp.h"      /* Lvgl_lock / Lvgl_unlock */
#include "ui_stock.h"
#include "stock_service.h"
#include "wifi_conn.h"

static const char *TAG = "app";
static lv_obj_t  *s_screen;

/* cJSON tree for Finnhub metric=all is large -> keep it out of internal RAM. */
static void *psram_malloc(size_t sz) {
    return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void UserApp_AppInit(void) {
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    cJSON_Hooks hooks = { .malloc_fn = psram_malloc, .free_fn = free };
    cJSON_InitHooks(&hooks);

    wifi_conn_start();
}

void UserApp_UiInit(void) {
    s_screen = lv_obj_create(NULL);
    lv_screen_load(s_screen);
    ui_stock_create(s_screen);
}

static void StockTask(void *arg) {
    (void)arg;
    if (!wifi_conn_wait(20000))
        ESP_LOGW(TAG, "WiFi not connected yet; will keep trying in background");
    wifi_conn_sync_time(10000);

    const char *sym = CONFIG_STOCK_SYMBOL;
    const char *key = CONFIG_STOCK_FINNHUB_API_KEY;

    int rotate  = CONFIG_STOCK_ROTATE_SECONDS;  if (rotate  < 1)      rotate  = 3;
    int refresh = CONFIG_STOCK_REFRESH_SECONDS; if (refresh < rotate) refresh = rotate;
    int ticks_per_refresh = refresh / rotate;   if (ticks_per_refresh < 1) ticks_per_refresh = 1;

    static stock_data_t data;     /* static: keep off the task stack */
    int page = 0;
    int tick = ticks_per_refresh; /* force an immediate first fetch */

    for (;;) {
        if (tick >= ticks_per_refresh) {
            int ok = stock_service_fetch(sym, key, &data);
            ESP_LOGI(TAG, "fetch %d/4  price=%.2f (%+.2f%%)",
                     ok, data.quote.price, data.quote.percent);
            if (Lvgl_lock(-1)) { ui_stock_update(&data); Lvgl_unlock(); }
            tick = 0;
        }
        if (Lvgl_lock(-1)) { ui_stock_show_page(page); Lvgl_unlock(); }
        page = (page + 1) % UI_STOCK_PAGE_COUNT;
        tick++;
        vTaskDelay(pdMS_TO_TICKS(rotate * 1000));
    }
}

void UserApp_TaskInit(void) {
    /* large stack: TLS handshake + JSON parsing run in this context */
    xTaskCreatePinnedToCore(StockTask, "stock", 16 * 1024, NULL, 3, NULL, 1);
}
