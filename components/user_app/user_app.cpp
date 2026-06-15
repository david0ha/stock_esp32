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
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>

#include "sdkconfig.h"
#include "cJSON.h"

#include "user_app.h"      /* prov_config_t */
#include "lvgl_bsp.h"      /* Lvgl_lock / Lvgl_unlock */
#include "ui_stock.h"
#include "stock_service.h"
#include "buttons.h"       /* USER/BOOT button events */

static const char *TAG = "app";
static lv_obj_t  *s_screen;

/* The active watchlist, copied from provisioning so it outlives app_main's
 * stack for the lifetime of the tasks. */
static prov_config_t s_cfg;

/*
 * Responsiveness comes from splitting input/render from the network:
 *
 *   StockTask  — input + render only (small stack, higher prio). Mutates the
 *                nav state and paints from the per-ticker cache; never blocks
 *                on the network, so a button press is handled the instant it
 *                arrives.
 *   FetchTask  — NUM_FETCHERS of them run the slow HTTPS/JSON downloads in
 *                parallel so the whole watchlist warms up quickly.
 *
 * Work is a set of per-ticker flags, not a FIFO, so duplicate refresh requests
 * collapse and the on-screen ticker is always fetched first:
 *   s_need[i] — ticker i wants a (re)download
 *   s_busy[i] — a worker is downloading ticker i right now (don't double-fetch)
 *   s_valid[i]/s_time_us[i] — cache slot has data / when it was fetched
 * A counting semaphore (s_fetch_wake) parks idle workers until work appears.
 * s_mtx guards all of the above plus s_sym_index / s_page.
 */
#define NUM_FETCHERS 2

/* The heavy data (metric=all ~240KB, intraday chart, week of news) changes
 * slowly, so we only re-download it this often. Ticker switches and periodic
 * refreshes in between just update the small realtime quote, which is what
 * actually moves — so a switch/refresh is one tiny request, not four. */
#define FULL_REFRESH_SECONDS 300

static QueueHandle_t     s_btn_queue;     /* USER/BOOT presses */
static SemaphoreHandle_t s_mtx;
static SemaphoreHandle_t s_fetch_wake;

static stock_data_t *s_cache;                       /* [s_ticker_n], PSRAM          */
static stock_data_t *s_scratch[NUM_FETCHERS];       /* per-worker download buffer   */
static bool          s_valid[PROV_MAX_TICKERS];
static bool          s_need[PROV_MAX_TICKERS];
static bool          s_busy[PROV_MAX_TICKERS];
static int64_t       s_time_us[PROV_MAX_TICKERS];   /* last fetch (full or quote)   */
static int64_t       s_full_us[PROV_MAX_TICKERS];   /* last FULL fetch (0 = never)  */
static size_t        s_ticker_n = 1;                /* watchlist length (>=1)       */

static int s_sym_index;   /* which ticker is on screen  */
static int s_page;        /* 0=chart, 1=news, 2=metrics  */

/* Human-readable names for the 3 ui_stock pages, for logging. */
static const char *kPageName[UI_STOCK_PAGE_COUNT] = { "CHART", "NEWS", "METRICS" };

static inline void state_lock(void)   { xSemaphoreTake(s_mtx, portMAX_DELAY); }
static inline void state_unlock(void) { xSemaphoreGive(s_mtx); }

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

/* Paint the currently selected ticker/page from the cache. Holds s_mtx across
 * the LVGL work so a concurrent cache store (FetchTask) can't tear the read;
 * the lock is only ever held for fast in-memory + LVGL ops, never the network. */
static void render_current(void) {
    state_lock();
    int  idx  = s_sym_index;
    int  page = s_page;
    bool have = s_valid[idx];
    if (Lvgl_lock(-1)) {
        if (have) ui_stock_update(&s_cache[idx]);   /* else: keep "loading" UI */
        ui_stock_show_page(page);
        Lvgl_unlock();
    }
    state_unlock();
}

/* Flag a ticker for (re)download and wake a worker. Idempotent: setting an
 * already-set flag collapses duplicate refresh requests into one fetch. */
static void request_fetch(int idx) {
    state_lock();
    s_need[idx] = true;
    state_unlock();
    xSemaphoreGive(s_fetch_wake);
}

/* Choose the next ticker to fetch: the on-screen one first (so a USER press is
 * honored immediately), otherwise any other ticker still waiting (prefetch).
 * Returns -1 if there's nothing to do. Marks the chosen slot busy and reports
 * via *out_full whether this needs a full download (first time, or the heavy
 * data has aged past FULL_REFRESH_SECONDS) or just a cheap quote refresh. */
static int claim_fetch_target(bool *out_full) {
    int target = -1;
    state_lock();
    int cur = s_sym_index;
    if (s_need[cur] && !s_busy[cur]) {
        target = cur;
    } else {
        for (int i = 0; i < (int)s_ticker_n; i++) {
            if (s_need[i] && !s_busy[i]) { target = i; break; }
        }
    }
    if (target >= 0) {
        s_need[target] = false;
        s_busy[target] = true;
        int64_t now = esp_timer_get_time();
        *out_full = !s_valid[target] || s_full_us[target] == 0 ||
                    (now - s_full_us[target] > (int64_t)FULL_REFRESH_SECONDS * 1000000);
    }
    state_unlock();
    return target;
}

/*
 * FetchTask — one of NUM_FETCHERS workers. Claims a ticker, downloads it (slow,
 * no lock held), stores it in the cache, and repaints only if that ticker is
 * still the one on screen. Workers park on s_fetch_wake when there's no work.
 */
static void FetchTask(void *arg) {
    int           wid     = (int)(intptr_t)arg;
    stock_data_t *scratch = s_scratch[wid];
    const char   *key     = CONFIG_STOCK_FINNHUB_API_KEY;

    for (;;) {
        bool full = true;
        int idx = claim_fetch_target(&full);
        if (idx < 0) { xSemaphoreTake(s_fetch_wake, portMAX_DELAY); continue; }

        const char *sym = prov_config_ticker_at(&s_cfg, (size_t)idx);
        if (!sym) sym = CONFIG_STOCK_SYMBOL;  /* empty watchlist -> Kconfig default */

        int ok;
        if (full) {
            ok = stock_service_fetch(sym, key, scratch);    /* all 4 endpoints */
        } else {
            /* Quote-only: start from the cached data so series/metrics/news
             * survive, then refresh just the realtime quote on top of it. */
            state_lock();
            *scratch = s_cache[idx];
            state_unlock();
            ok = stock_service_fetch_quote(sym, key, scratch);
        }
        int64_t now = esp_timer_get_time();

        state_lock();
        s_cache[idx]   = *scratch;
        s_valid[idx]   = true;
        s_time_us[idx] = now;
        if (full) s_full_us[idx] = now;
        s_busy[idx]    = false;
        bool is_current = (idx == s_sym_index);
        state_unlock();

        ESP_LOGI(TAG, "[w%d] %-5s %s ok=%d  price=%.2f (%+.2f%%)  %s",
                 wid, full ? "full" : "quote", sym, ok,
                 scratch->quote.price, scratch->quote.percent,
                 is_current ? "[on screen]" : "[cached]");

        if (is_current) render_current();
    }
}

/*
 * StockTask — input + render only (small stack, higher priority). Never blocks
 * on the network, so button presses are handled the instant they arrive:
 *   USER (GPIO18) -> next ticker; paints cached data now, fetches only if stale
 *   BOOT (GPIO0)  -> next view of the current ticker (pure local page swap)
 * On idle it kicks a background refresh of the on-screen ticker.
 */
static void StockTask(void *arg) {
    (void)arg;
    int refresh = CONFIG_STOCK_REFRESH_SECONDS; if (refresh < 1) refresh = 30;
    int64_t refresh_us = (int64_t)refresh * 1000000;

    ESP_LOGI(TAG, "controls: USER=next ticker, BOOT=next view | %u ticker(s)",
             (unsigned)s_ticker_n);

    /* Warm the whole watchlist up front so USER switches land on cached data.
     * The on-screen ticker (index 0) is fetched first by claim_fetch_target();
     * the rest fill in across the workers. */
    state_lock();
    for (int i = 0; i < (int)s_ticker_n; i++) s_need[i] = true;
    state_unlock();
    for (int i = 0; i < NUM_FETCHERS; i++) xSemaphoreGive(s_fetch_wake);

    render_current();   /* blank until the first fetch lands */

    for (;;) {
        button_event_t ev;
        if (xQueueReceive(s_btn_queue, &ev, pdMS_TO_TICKS(refresh * 1000)) == pdTRUE) {
            if (ev.id == BUTTON_USER) {        /* next ticker, same view */
                state_lock();
                s_sym_index = (s_sym_index + 1) % (int)s_ticker_n;
                int     idx   = s_sym_index;
                int64_t now   = esp_timer_get_time();
                bool    stale = !s_valid[idx] || (now - s_time_us[idx] > refresh_us);
                state_unlock();

                ESP_LOGI(TAG, "USER -> ticker %d/%u %s", idx + 1, (unsigned)s_ticker_n,
                         stale ? "(fetching)" : "(cached, instant)");
                render_current();              /* instant */
                if (stale) request_fetch(idx);
            } else if (ev.id == BUTTON_BOOT) { /* next view, same ticker (no network) */
                state_lock();
                s_page = (s_page + 1) % UI_STOCK_PAGE_COUNT;
                int page = s_page;
                state_unlock();

                ESP_LOGI(TAG, "BOOT -> view %s (%d/%d)", kPageName[page],
                         page + 1, UI_STOCK_PAGE_COUNT);
                render_current();              /* instant */
            }
        } else {
            /* idle: refresh whatever is on screen so prices/news stay live */
            state_lock();
            int idx = s_sym_index;
            state_unlock();
            ESP_LOGI(TAG, "auto-refresh ticker %d", idx + 1);
            request_fetch(idx);
        }
    }
}

void UserApp_TaskInit(const prov_config_t *cfg) {
    s_cfg = *cfg;
    s_ticker_n = cfg->ticker_count ? cfg->ticker_count : 1;

    /* Per-ticker cache + per-worker scratch live in PSRAM (each ~1.6KB). */
    s_cache = (stock_data_t *)heap_caps_calloc(s_ticker_n, sizeof(stock_data_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_cache) { ESP_LOGE(TAG, "cache alloc failed (%u slots)", (unsigned)s_ticker_n); return; }
    for (int i = 0; i < NUM_FETCHERS; i++) {
        s_scratch[i] = (stock_data_t *)heap_caps_malloc(sizeof(stock_data_t),
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_scratch[i]) { ESP_LOGE(TAG, "scratch alloc failed"); return; }
    }

    s_mtx        = xSemaphoreCreateMutex();
    s_fetch_wake = xSemaphoreCreateCounting(PROV_MAX_TICKERS + NUM_FETCHERS, 0);
    s_btn_queue  = xQueueCreate(8, sizeof(button_event_t));
    buttons_init(s_btn_queue);

    /* FetchTask workers: big stack for the TLS handshake + JSON parsing. */
    for (int i = 0; i < NUM_FETCHERS; i++) {
        char name[8];
        snprintf(name, sizeof(name), "fetch%d", i);
        xTaskCreatePinnedToCore(FetchTask, name, 16 * 1024, (void *)(intptr_t)i, 3, NULL, 1);
    }
    /* StockTask: input/render only -> small stack, higher prio for snappy buttons. */
    xTaskCreatePinnedToCore(StockTask, "ui", 4 * 1024, NULL, 4, NULL, 1);
}
