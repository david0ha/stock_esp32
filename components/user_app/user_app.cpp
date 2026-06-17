/*
 * user_app.cpp — app orchestration for the stock monitor.
 *
 *   AppInit:  route cJSON allocations to PSRAM.
 *   UiInit:   build the LVGL stock UI on a fresh screen.
 *   TaskInit: spawn the input/render task + parallel fetch workers. Navigation
 *             is button-driven: USER cycles the watchlist, BOOT cycles the
 *             home / chart / news / metrics views of the current ticker.
 *
 * WiFi bring-up, NVS, and the post-connect clock sync are owned by the
 * `provisioning` component; on-board sensors (SHTC3 / RTC / battery) by
 * `board_io`. By the time TaskInit runs the network is up and the clock is set,
 * so the render loop just paints from cache and ticks the home clock/weather.
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

#include <time.h>

#include "user_app.h"      /* prov_config_t */
#include "lvgl_bsp.h"      /* Lvgl_lock / Lvgl_unlock */
#include "ui_stock.h"
#include "ui_econ.h"       /* economic-calendar overlay */
#include "stock_service.h"
#include "econ_service.h"  /* FMP economic calendar */
#include "econ_parse.h"    /* econ_week_range for the loading label */
#include "buttons.h"       /* USER/BOOT button events */
#include "board_io.h"      /* SHTC3 / RTC / battery */

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
 *                arrives. Also ticks the home clock + sensors on a short timer.
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

/* How often the render loop wakes (when idle) to refresh the home clock and the
 * temperature/humidity/battery readings. The clock shows HH:MM and the sensors
 * drift slowly, so this only needs to be a fraction of a minute — short enough
 * to keep the minute honest, long enough not to thrash the reflective panel or
 * spin the SHTC3/ADC for a display that can't change faster than once a minute. */
#define HOME_TICK_SECONDS 15

/* Pressing KEY (USER) + BOOT together toggles the economic-calendar view. The
 * two GPIOs fire independent edge events, so on the first event we poll the real
 * pin state (buttons_both_pressed) every CHORD_POLL_MS up to CHORD_WINDOW_MS,
 * stopping as soon as both are held. Polling GPIO (not event timing) means two
 * quick *separate* presses are never mistaken for a chord, and the window
 * tolerates a second finger that lands a little late. A genuine single press
 * costs at most CHORD_WINDOW_MS of latency. */
#define CHORD_POLL_MS    15
#define CHORD_WINDOW_MS  120

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

static int s_sym_index;   /* which ticker is on screen                       */
static int s_page;        /* 0=home, 1=chart, 2=news, 3=metrics (starts home) */

/* Economic-calendar overlay state, all under s_mtx. s_econ_mode is the field
 * actually read cross-task (FetchTask / render_current skip painting stock under
 * the overlay); s_econ_week / s_econ_page are touched only by StockTask but ride
 * the same lock for brevity. s_econ itself is StockTask-only — written by
 * render_econ, read straight into LVGL right after — so never read it elsewhere. */
static bool            s_econ_mode;   /* calendar overlay shown instead of stock  */
static int             s_econ_week;   /* week offset; cycles within the month     */
static int             s_econ_page;   /* page within the week (KEY scrolls events) */
static econ_calendar_t s_econ;        /* last fetched week (BSS, ~7KB)            */

/* Human-readable names for the ui_stock pages, for logging. */
static const char *kPageName[UI_STOCK_PAGE_COUNT] = { "HOME", "CHART", "NEWS", "METRICS" };

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
    ui_econ_create(s_screen);   /* hidden overlay; shown on the KEY+BOOT chord */
}

/* Sample the on-board sensors into `env` for the home page + battery chip. */
static void read_env(ui_env_t *env) {
    float t = 0, h = 0;
    env->env_valid = board_io_read_env(&t, &h);
    env->temp_c    = t;
    env->humidity  = h;
    env->battery_v   = board_io_battery_voltage();
    env->battery_pct = board_io_battery_percent();
    env->battery_valid = env->battery_v > 0.1f;
}

/* Refresh the home clock + sensors (cheap, local). Ticks the clock via
 * ui_stock_update_env even on the stock pages so the top-bar battery chip and
 * the (hidden) home clock are always current when the user navigates back. */
static void tick_home_env(void) {
    ui_env_t env;
    read_env(&env);
    if (Lvgl_lock(-1)) {
        ui_stock_update_env(&env);
        Lvgl_unlock();
    }
}

/* Paint the currently selected ticker/page from the cache. Holds s_mtx across
 * the LVGL work so a concurrent cache store (FetchTask) can't tear the read;
 * the lock is only ever held for fast in-memory + LVGL ops, never the network. */
static void render_current(void) {
    state_lock();
    /* The calendar overlay is up: don't paint the stock UI underneath. Checked
     * under s_mtx (set by toggle_econ) so a concurrent toggle can't slip a stock
     * repaint in after the check — no TOCTOU. */
    if (s_econ_mode) { state_unlock(); return; }
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

        /* Download with no lock held. A full fetch fills the whole struct; the
         * quote-only refresh writes just scratch->quote, and only on success
         * (stock_service_fetch_quote is transactional), so the other fields of
         * scratch are never read on the quote path. */
        int ok = full ? stock_service_fetch(sym, key, scratch)
                       : stock_service_fetch_quote(sym, key, scratch);
        int64_t now = esp_timer_get_time();

        state_lock();
        bool stored = false;
        if (full) {
            s_cache[idx]   = *scratch;              /* whole struct */
            s_valid[idx]   = true;
            s_time_us[idx] = now;
            s_full_us[idx] = now;
            stored = true;
        } else if (ok) {
            s_cache[idx].quote = scratch->quote;    /* graft just the quote */
            s_time_us[idx]     = now;
            stored = true;
        }   /* a failed quote refresh leaves the cache (and its staleness) intact */
        s_busy[idx] = false;
        double px = s_cache[idx].quote.price, pct = s_cache[idx].quote.percent;
        bool is_current = (idx == s_sym_index);
        state_unlock();

        ESP_LOGI(TAG, "[w%d] %-5s %s ok=%d  price=%.2f (%+.2f%%)  %s",
                 wid, full ? "full" : "quote", sym, ok, px, pct,
                 is_current ? "[on screen]" : "[cached]");

        /* render_current() self-skips while the calendar overlay is up. */
        if (is_current && stored) render_current();
    }
}

/* Pages in the loaded week (>=1; 0 for an invalid/errored week). */
static int econ_pages(void) {
    return econ_page_count(s_econ.valid ? s_econ.count : 0);
}

/* Paint the economic calendar. Runs in the UI task (StockTask), so that task
 * carries a big stack for the TLS handshake + JSON parse. When `refetch`, pulls
 * the current s_econ_week from the network (with a "Loading..." frame, TLS reused
 * across weeks); otherwise it just re-pages the already-fetched week — so KEY
 * scrolls through this week's events with no network hit. */
static void render_econ(bool refetch) {
    if (refetch) {
        time_t now = time(NULL);
        long   tz  = econ_local_tz_off(now);
        state_lock();
        int wk = s_econ_week;
        state_unlock();

        char from[12], to[12], label[ECON_LABEL_MAXLEN];
        econ_week_range(now, tz, wk, from, sizeof from, to, sizeof to, label, sizeof label);
        if (Lvgl_lock(-1)) { ui_econ_set_loading(label); Lvgl_unlock(); }

        econ_service_fetch(CONFIG_STOCK_FMP_API_KEY, now, tz, wk,
                           CONFIG_STOCK_ECON_MIN_IMPACT, &s_econ);
        ESP_LOGI(TAG, "econ wk=%d [%s] valid=%d count=%d/%d %s", wk, s_econ.week_label,
                 s_econ.valid, s_econ.count, s_econ.total_matched,
                 s_econ.valid ? "" : s_econ.error);
    }

    /* Clamp the page into the (possibly new) event count. */
    int pages = econ_pages();
    state_lock();
    if (s_econ_page >= pages) s_econ_page = pages - 1;
    if (s_econ_page < 0)      s_econ_page = 0;
    int page = s_econ_page;
    state_unlock();

    if (Lvgl_lock(-1)) { ui_econ_set_calendar(&s_econ, page); Lvgl_unlock(); }
}

/* Toggle between the stock view and the economic-calendar overlay (KEY+BOOT). */
static void toggle_econ(void) {
    state_lock();
    s_econ_mode = !s_econ_mode;
    bool entering = s_econ_mode;
    if (entering) { s_econ_week = 0; s_econ_page = 0; }  /* (re)enter on this week */
    state_unlock();

    ESP_LOGI(TAG, "KEY+BOOT -> %s", entering ? "economic calendar" : "stock view");
    if (entering) {
        if (Lvgl_lock(-1)) { ui_econ_show(true); Lvgl_unlock(); }
        render_econ(true);
    } else {
        if (Lvgl_lock(-1)) { ui_econ_show(false); Lvgl_unlock(); }
        render_current();
    }
}

/* Handle one (non-chord) button press, dispatched by the active view. */
static void handle_press(button_id_t id, int64_t refresh_us) {
    if (s_econ_mode) {
        if (id == BUTTON_USER) {       /* KEY: next page of events, cycling in week */
            int pages = econ_pages();
            state_lock();
            s_econ_page = (s_econ_page + 1) % pages;
            int page = s_econ_page;
            state_unlock();
            ESP_LOGI(TAG, "KEY -> econ page %d/%d", page + 1, pages);
            render_econ(false);        /* re-page only, no network */
        } else {                       /* BOOT: next week, cycling within the month */
            time_t now = time(NULL);
            int wmin, wmax;
            econ_month_week_span(now, econ_local_tz_off(now), &wmin, &wmax);
            state_lock();
            s_econ_week = (s_econ_week >= wmax) ? wmin : s_econ_week + 1;
            s_econ_page = 0;
            int wk = s_econ_week;
            state_unlock();
            ESP_LOGI(TAG, "BOOT -> econ week offset %d (month %d..%d)", wk, wmin, wmax);
            render_econ(true);
        }
        return;
    }

    if (id == BUTTON_USER) {           /* next ticker, same view */
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
    } else {                           /* BOOT: next view, same ticker (no network) */
        state_lock();
        s_page = (s_page + 1) % UI_STOCK_PAGE_COUNT;
        int page = s_page;
        state_unlock();

        ESP_LOGI(TAG, "BOOT -> view %s (%d/%d)", kPageName[page],
                 page + 1, UI_STOCK_PAGE_COUNT);
        render_current();              /* instant */
    }
}

/*
 * StockTask — input + render only (higher priority), but it also performs the
 * economic-calendar fetch inline, so it carries a big stack (TLS + JSON). Button
 * presses are handled the instant they arrive:
 *   USER (GPIO18) -> next ticker; paints cached data now, fetches only if stale
 *   BOOT (GPIO0)  -> next view (home/chart/news/metrics), pure local page swap
 *   USER+BOOT     -> toggle the calendar (in it: KEY=next events, BOOT=next week)
 * Between presses it wakes every HOME_TICK_SECONDS to keep the home clock and
 * battery chip live, and kicks a background refresh of the on-screen ticker on
 * the (slower) REFRESH cadence.
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

    /* Prime the home page with a real clock + sensor reading, then show it
     * (page 0 = home). The quote stays blank until the first fetch lands. */
    tick_home_env();
    render_current();

    int64_t last_refresh_us = esp_timer_get_time();

    for (;;) {
        button_event_t ev;
        if (xQueueReceive(s_btn_queue, &ev, pdMS_TO_TICKS(HOME_TICK_SECONDS * 1000)) == pdTRUE) {
            /* Poll the pins for a chord (both held), stopping early once seen;
             * tolerates a slightly-late second finger without delaying a true
             * single press by more than CHORD_WINDOW_MS. */
            bool chord = buttons_both_pressed();
            for (int w = 0; !chord && w < CHORD_WINDOW_MS; w += CHORD_POLL_MS) {
                vTaskDelay(pdMS_TO_TICKS(CHORD_POLL_MS));
                chord = buttons_both_pressed();
            }
            if (chord) {
                button_event_t drop;   /* discard the partner edge(s) of the chord */
                while (xQueueReceive(s_btn_queue, &drop, 0) == pdTRUE) { }
                toggle_econ();
            } else {
                handle_press(ev.id, refresh_us);
            }
        } else {
            /* Calendar view is static between presses — skip the home/network tick. */
            if (s_econ_mode) continue;

            /* Idle tick: keep the home clock + sensors current (cheap, local). */
            tick_home_env();

            /* Refresh the on-screen ticker on the slower network cadence so
             * prices/news stay live without re-downloading every few seconds. */
            int64_t now = esp_timer_get_time();
            if (now - last_refresh_us >= refresh_us) {
                last_refresh_us = now;
                state_lock();
                int idx = s_sym_index;
                state_unlock();
                ESP_LOGI(TAG, "auto-refresh ticker %d", idx + 1);
                request_fetch(idx);
            }
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
    s_btn_queue  = xQueueCreate(16, sizeof(button_event_t));
    buttons_init(s_btn_queue);

    /* FetchTask workers: big stack for the TLS handshake + JSON parsing. */
    for (int i = 0; i < NUM_FETCHERS; i++) {
        char name[8];
        snprintf(name, sizeof(name), "fetch%d", i);
        xTaskCreatePinnedToCore(FetchTask, name, 16 * 1024, (void *)(intptr_t)i, 3, NULL, 1);
    }
    /* StockTask: input/render + inline economic-calendar fetch, so it needs a big
     * stack (TLS handshake + JSON parse) like the fetch workers. Higher prio so
     * button presses stay snappy. */
    xTaskCreatePinnedToCore(StockTask, "ui", 16 * 1024, NULL, 4, NULL, 1);
}
