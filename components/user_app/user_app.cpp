/*
 * user_app.cpp — app orchestration for the stock monitor.
 *
 *   AppInit:  route cJSON allocations to PSRAM.
 *   UiInit:   build the LVGL stock UI on a fresh screen.
 *   TaskInit: spawn the task that fetches data and rotates the 3 pages,
 *             cycling through the provisioned watchlist of tickers.
 *
 * WiFi bring-up, NVS, and the post-connect clock sync are owned by the
 * `provisioning` component; by the time TaskInit runs the network is up and the
 * clock is set, so this task is a pure fetch/render loop.
 *
 * The portable core (parsers, service, UI) lives in the stock_core component
 * and is the same code verified by the host tests and the desktop simulator.
 */
#include <stdio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_heap_caps.h>

#include "sdkconfig.h"
#include "cJSON.h"

#include "user_app.h"      /* prov_config_t */
#include "lvgl_bsp.h"      /* Lvgl_lock / Lvgl_unlock */
#include "ui_stock.h"
#include "stock_service.h"
#include "board_io.h"      /* SHTC3 / RTC / battery */

static const char *TAG = "app";
static lv_obj_t  *s_screen;

/* The active watchlist, copied from provisioning so it outlives app_main's
 * stack for the lifetime of the fetch task. */
static prov_config_t s_cfg;

/* cJSON tree for Finnhub metric=all is large -> keep it out of internal RAM. */
static void *psram_malloc(size_t sz) {
    return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void UserApp_AppInit(void) {
    /* Route the large cJSON parse tree (Finnhub metric=all is ~240KB) to PSRAM.
     * NVS is initialized by the provisioning component, which runs right after this. */
    cJSON_Hooks hooks = { .malloc_fn = psram_malloc, .free_fn = free };
    cJSON_InitHooks(&hooks);
}

void UserApp_UiInit(void) {
    /* Swap the provisioning status screen for the stock UI, freeing the old one
     * (and its widgets) instead of leaking it for the process lifetime. */
    lv_obj_t *prev = lv_screen_active();
    s_screen = lv_obj_create(NULL);
    lv_screen_load(s_screen);
    if (prev && prev != s_screen) lv_obj_delete(prev);
    ui_stock_create(s_screen);
}

/* Rotation order: return to the home (clock/weather) page between every stock
 * page so the clock stays the dominant screen (pages: 1=chart 2=news 3=metrics). */
static const int PAGE_SEQ[] = { UI_STOCK_PAGE_HOME, 1,
                                UI_STOCK_PAGE_HOME, 2,
                                UI_STOCK_PAGE_HOME, 3 };
#define PAGE_SEQ_LEN ((int)(sizeof(PAGE_SEQ) / sizeof(PAGE_SEQ[0])))

static void read_env(ui_env_t *env) {
    float t = 0, h = 0;
    env->env_valid = board_io_read_env(&t, &h);
    env->temp_c    = t;
    env->humidity  = h;
    env->battery_v   = board_io_battery_voltage();
    env->battery_pct = board_io_battery_percent();
    env->battery_valid = env->battery_v > 0.1f;
}

static void StockTask(void *arg) {
    (void)arg;
    const char *key = CONFIG_STOCK_FINNHUB_API_KEY;

    int rotate  = CONFIG_STOCK_ROTATE_SECONDS;  if (rotate  < 1)      rotate  = 3;
    int refresh = CONFIG_STOCK_REFRESH_SECONDS; if (refresh < rotate) refresh = rotate;
    int ticks_per_refresh = refresh / rotate;   if (ticks_per_refresh < 1) ticks_per_refresh = 1;

    static stock_data_t data;     /* static: keep off the task stack */
    int    seq  = 0;
    int    tick = ticks_per_refresh; /* force an immediate first fetch */
    size_t sym_index = 0;            /* advances each refresh -> cycles the watchlist */

    for (;;) {
        if (tick >= ticks_per_refresh) {
            const char *sym = prov_config_ticker_at(&s_cfg, sym_index);
            if (!sym) sym = CONFIG_STOCK_SYMBOL; /* empty watchlist -> Kconfig default */
            int ok = stock_service_fetch(sym, key, &data);
            ESP_LOGI(TAG, "fetch %s %d/4  price=%.2f (%+.2f%%)",
                     sym, ok, data.quote.price, data.quote.percent);
            if (Lvgl_lock(-1)) { ui_stock_update(&data); Lvgl_unlock(); }
            tick = 0;
            sym_index++;
        }

        /* refresh sensors + clock every tick so the home page is always current */
        ui_env_t env;
        read_env(&env);
        if (Lvgl_lock(-1)) {
            ui_stock_update_env(&env);
            ui_stock_show_page(PAGE_SEQ[seq]);
            Lvgl_unlock();
        }
        seq = (seq + 1) % PAGE_SEQ_LEN;
        tick++;
        vTaskDelay(pdMS_TO_TICKS(rotate * 1000));
    }
}

void UserApp_TaskInit(const prov_config_t *cfg) {
    s_cfg = *cfg;
    /* large stack: TLS handshake + JSON parsing run in this context */
    xTaskCreatePinnedToCore(StockTask, "stock", 16 * 1024, NULL, 3, NULL, 1);
}
