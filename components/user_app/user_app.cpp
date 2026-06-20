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
#include <string.h>

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
#include "prov_store.h"        /* persist watchlist changes to NVS */
#include "user_app_control.h"  /* companion-app HTTP control bridge (implemented in this file) */

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

/* Commands injected by the companion-app HTTP server (components/stock_api) through the
 * user_app_control bridge. They are applied on StockTask via the same code paths as a button
 * press, so there is never any cross-task LVGL access or new lock ordering. */
typedef enum {
    APP_CMD_SELECT_INDEX,   /* ival = watchlist index                    */
    APP_CMD_SET_PAGE,       /* ival = page 0..3                          */
    APP_CMD_SET_ECON,       /* bval = overlay on/off, ival = week        */
    APP_CMD_REFRESH,        /* bval = refresh whole watchlist vs current */
    APP_CMD_SET_WATCHLIST,  /* text = normalized comma-separated symbols */
    APP_CMD_SET_KEYS,       /* set_<x> + key fields below                */
} app_cmd_kind_t;

typedef struct {
    app_cmd_kind_t kind;
    int  ival;
    bool bval;
    char text[PROV_MAX_TICKERS * (PROV_TICKER_MAX_LEN + 1) + 1];  /* SET_WATCHLIST: csv */
    /* SET_KEYS: only the flagged fields are applied (so the settings screen can update one key
     * without clearing the others); an empty flagged string clears that key -> Kconfig default. */
    bool set_finnhub, set_fmp, set_econ;
    char finnhub[PROV_FINNHUB_KEY_MAX + 1];
    char fmp[PROV_FMP_KEY_MAX + 1];
    char econ_url[PROV_ECON_URL_MAX + 1];
} app_cmd_t;

static QueueHandle_t     s_btn_queue;     /* USER/BOOT presses */
static QueueHandle_t     s_cmd_queue;     /* control commands from the HTTP server */
static QueueSetHandle_t  s_queue_set;     /* wakes StockTask on a button OR a command */
static SemaphoreHandle_t s_mtx;
static SemaphoreHandle_t s_fetch_wake;

/* Last sensor read, cached under s_mtx so the HTTP snapshot (user_app_snapshot) can report it
 * without touching the SHTC3/ADC from another task. */
static ui_env_t  s_last_env;
/* Refresh staleness bound, mirrored from CONFIG_STOCK_REFRESH_SECONDS so handle_cmd can reuse
 * the same "is this slot stale" rule the button path uses. Set once by StockTask. */
static int64_t   s_refresh_us = 30LL * 1000000;

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

/* Bumped under s_mtx by apply_watchlist on every live watchlist replace. A FetchTask captures
 * it at claim time and, if it changed by store time, discards its (now stale-symbol) result so
 * an in-flight fetch can't paint the previous symbol's data into a repurposed cache slot. */
static uint32_t s_generation;

/* Economic-calendar overlay state. s_econ_mode / s_econ_week are guarded by
 * s_mtx (read cross-task by FetchTask). s_econ itself is only ever touched by
 * StockTask — written by render_econ, read synchronously into LVGL right after —
 * so it needs no lock; keep it that way (don't read s_econ from another task). */
static bool            s_econ_mode;   /* calendar overlay shown instead of stock */
static int             s_econ_week;   /* 0 = this week, -1 = previous, +1 = next  */
static econ_calendar_t s_econ;        /* last fetched week (BSS, ~1.4KB)          */

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
    /* Cache for the HTTP snapshot (read cross-task by user_app_snapshot under s_mtx). */
    if (s_mtx) { state_lock(); s_last_env = env; state_unlock(); }
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
static int claim_fetch_target(bool *out_full, char *out_sym, size_t sym_sz,
                              char *out_key, size_t key_sz, uint32_t *out_gen) {
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
        /* Snapshot the symbol + generation under the lock so the lock-free download below never
         * dereferences s_cfg, which apply_watchlist may rewrite concurrently (torn read). */
        const char *sym = prov_config_ticker_at(&s_cfg, (size_t)target);
        strlcpy(out_sym, sym ? sym : CONFIG_STOCK_SYMBOL, sym_sz);
        strlcpy(out_key, s_cfg.finnhub_key[0] ? s_cfg.finnhub_key : CONFIG_STOCK_FINNHUB_API_KEY, key_sz);
        *out_gen = s_generation;
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

    for (;;) {
        bool full = true;
        char sym[STOCK_SYMBOL_MAXLEN];        /* private copy; never points into s_cfg     */
        char key[PROV_FINNHUB_KEY_MAX + 1];   /* runtime Finnhub key snapshot (app-updated) */
        uint32_t gen = 0;
        int idx = claim_fetch_target(&full, sym, sizeof(sym), key, sizeof(key), &gen);
        if (idx < 0) { xSemaphoreTake(s_fetch_wake, portMAX_DELAY); continue; }

        /* Download with no lock held, against the private symbol copy. A full fetch fills the
         * whole struct; the quote-only refresh writes just scratch->quote, and only on success
         * (stock_service_fetch_quote is transactional), so the other fields of scratch are
         * never read on the quote path. */
        int ok = full ? stock_service_fetch(sym, key, scratch)
                       : stock_service_fetch_quote(sym, key, scratch);
        int64_t now = esp_timer_get_time();

        state_lock();
        /* If the watchlist was replaced while this download was in flight, slot `idx` now holds
         * a different symbol: drop this result (it is the OLD symbol's data) rather than paint it
         * into the repurposed slot. apply_watchlist already set s_need[idx], so the new symbol is
         * re-fetched promptly. */
        if (s_generation != gen) {
            s_busy[idx] = false;
            state_unlock();
            ESP_LOGI(TAG, "[w%d] %-5s %s dropped (watchlist changed)", wid, full ? "full" : "quote", sym);
            continue;
        }
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

/* Fetch + paint the economic calendar for the current s_econ_week. Runs in the
 * UI task (StockTask), so that task carries a big stack for the TLS handshake +
 * JSON parse. Paints a "Loading..." frame first, then the events or the error
 * message. The TLS connection to FMP is reused across week navigations. */
static void render_econ(void) {
    time_t now = time(NULL);
    long   tz  = econ_local_tz_off(now);
    state_lock();
    int wk = s_econ_week;
    state_unlock();

    char from[12], to[12], label[ECON_LABEL_MAXLEN];
    econ_week_range(now, tz, wk, from, sizeof from, to, sizeof to, label, sizeof label);
    if (Lvgl_lock(-1)) { ui_econ_set_loading(label); Lvgl_unlock(); }

    /* Runtime FMP key + econ URL from the app (NVS), falling back to the Kconfig defaults. Read
     * on StockTask, which also owns apply_keys, so these two need no lock. */
    const char *fmp  = s_cfg.fmp_key[0]  ? s_cfg.fmp_key  : CONFIG_STOCK_FMP_API_KEY;
    const char *eurl = s_cfg.econ_url[0] ? s_cfg.econ_url : NULL;
    econ_service_fetch(fmp, eurl, now, tz, wk, CONFIG_STOCK_ECON_MIN_IMPACT, &s_econ);
    ESP_LOGI(TAG, "econ wk=%d [%s] valid=%d count=%d/%d %s", wk, s_econ.week_label,
             s_econ.valid, s_econ.count, s_econ.total_matched,
             s_econ.valid ? "" : s_econ.error);
    if (Lvgl_lock(-1)) { ui_econ_set_calendar(&s_econ); Lvgl_unlock(); }
}

/* Toggle between the stock view and the economic-calendar overlay (KEY+BOOT). */
static void toggle_econ(void) {
    state_lock();
    s_econ_mode = !s_econ_mode;
    bool entering = s_econ_mode;
    if (entering) s_econ_week = 0;     /* always (re)enter on the current week */
    state_unlock();

    ESP_LOGI(TAG, "KEY+BOOT -> %s", entering ? "economic calendar" : "stock view");
    if (entering) {
        if (Lvgl_lock(-1)) { ui_econ_show(true); Lvgl_unlock(); }
        render_econ();
    } else {
        if (Lvgl_lock(-1)) { ui_econ_show(false); Lvgl_unlock(); }
        render_current();
    }
}

/* ---- shared actions: used by BOTH the buttons (handle_press) and the app (handle_cmd) ---- */

/* Select watchlist index `idx` (caller guarantees 0 <= idx < s_ticker_n): paint cached data
 * immediately, and kick a background fetch only if that slot is stale or empty. */
static void apply_select(int idx, int64_t refresh_us) {
    state_lock();
    s_sym_index = idx;
    int64_t now = esp_timer_get_time();
    bool stale = !s_valid[idx] || (now - s_time_us[idx] > refresh_us);
    state_unlock();
    render_current();                  /* instant */
    if (stale) request_fetch(idx);
}

/* Switch the view page (caller guarantees 0 <= page < UI_STOCK_PAGE_COUNT). render_current
 * self-skips while the econ overlay is up, so the page is simply staged until it is dismissed. */
static void apply_page(int page) {
    state_lock();
    s_page = page;
    state_unlock();
    render_current();
}

/* Show/hide the economic-calendar overlay explicitly (the app's set_econ). Mirrors toggle_econ
 * but sets an absolute state + week instead of flipping. */
static void apply_econ(bool mode, int week) {
    state_lock();
    s_econ_mode = mode;
    if (mode) s_econ_week = week;
    state_unlock();
    if (mode) {
        if (Lvgl_lock(-1)) { ui_econ_show(true); Lvgl_unlock(); }
        render_econ();
    } else {
        if (Lvgl_lock(-1)) { ui_econ_show(false); Lvgl_unlock(); }
        render_current();
    }
}

/* Replace the watchlist live (no reboot). Parses `csv`, swaps it into the active config under
 * the lock, resets the per-slot cache for the new set, persists to NVS (keeping ssid/password),
 * and warms every slot. A list that parses empty is ignored (the bridge already rejected it). */
static void apply_watchlist(const char *csv) {
    prov_config_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    size_t n = prov_tickers_parse(&tmp, csv);
    if (n == 0) return;

    state_lock();
    for (size_t i = 0; i < PROV_MAX_TICKERS; i++) {
        s_cfg.tickers[i][0] = '\0';
        if (i < n) strlcpy(s_cfg.tickers[i], tmp.tickers[i], sizeof(s_cfg.tickers[i]));
        /* Reset the cache slot for the new set; an in-flight fetch (s_busy) is left to finish
         * and is harmless — its slot is re-fetched via s_need below. */
        s_valid[i]   = false;
        s_need[i]    = (i < n);
        s_time_us[i] = 0;
        s_full_us[i] = 0;
        memset(&s_cache[i], 0, sizeof(s_cache[i]));
    }
    s_cfg.ticker_count = n;
    s_ticker_n = n;
    if (s_sym_index >= (int)s_ticker_n) s_sym_index = 0;
    s_generation++;   /* invalidate any fetch that was claimed against the old watchlist */
    state_unlock();

    if (!prov_store_save(&s_cfg)) {
        ESP_LOGW(TAG, "watchlist change: NVS save failed (will not survive reboot)");
    }
    for (int i = 0; i < NUM_FETCHERS; i++) xSemaphoreGive(s_fetch_wake);
    render_current();
    ESP_LOGI(TAG, "watchlist replaced with %u symbol(s) via app", (unsigned)n);
}

/* Update the runtime data-source keys/URL (from the app settings), persist to NVS, and force a
 * re-fetch with the new credentials. Bumps the generation so an in-flight fetch using the old key
 * is discarded. Only the flagged fields change; an empty flagged field clears that key. */
static void apply_keys(const app_cmd_t *c) {
    state_lock();
    if (c->set_finnhub) strlcpy(s_cfg.finnhub_key, c->finnhub,  sizeof(s_cfg.finnhub_key));
    if (c->set_fmp)     strlcpy(s_cfg.fmp_key,     c->fmp,      sizeof(s_cfg.fmp_key));
    if (c->set_econ)    strlcpy(s_cfg.econ_url,    c->econ_url, sizeof(s_cfg.econ_url));
    s_generation++;
    for (int i = 0; i < (int)s_ticker_n; i++) s_need[i] = true;   /* re-fetch with the new key */
    state_unlock();

    if (!prov_store_save(&s_cfg)) {
        ESP_LOGW(TAG, "keys change: NVS save failed (will not survive reboot)");
    }
    for (int i = 0; i < NUM_FETCHERS; i++) xSemaphoreGive(s_fetch_wake);
    ESP_LOGI(TAG, "data-source keys updated via app (finnhub=%d fmp=%d econ_url=%d)",
             c->set_finnhub, c->set_fmp, c->set_econ);
}

/* Apply one control command from the HTTP server (runs on StockTask). */
static void handle_cmd(const app_cmd_t *c) {
    switch (c->kind) {
        case APP_CMD_SELECT_INDEX: {
            state_lock();
            bool ok = (c->ival >= 0 && c->ival < (int)s_ticker_n);
            state_unlock();
            if (ok) {
                ESP_LOGI(TAG, "app -> select ticker %d", c->ival + 1);
                apply_select(c->ival, s_refresh_us);
            }
            break;
        }
        case APP_CMD_SET_PAGE:
            if (c->ival >= 0 && c->ival < UI_STOCK_PAGE_COUNT) {
                ESP_LOGI(TAG, "app -> page %s", kPageName[c->ival]);
                apply_page(c->ival);
            }
            break;
        case APP_CMD_SET_ECON:
            ESP_LOGI(TAG, "app -> econ %s week=%d", c->bval ? "on" : "off", c->ival);
            apply_econ(c->bval, c->ival);
            break;
        case APP_CMD_REFRESH:
            if (c->bval) {
                state_lock();
                for (int i = 0; i < (int)s_ticker_n; i++) s_need[i] = true;
                state_unlock();
                for (int i = 0; i < NUM_FETCHERS; i++) xSemaphoreGive(s_fetch_wake);
                ESP_LOGI(TAG, "app -> refresh all");
            } else {
                state_lock();
                int idx = s_sym_index;
                state_unlock();
                ESP_LOGI(TAG, "app -> refresh current (%d)", idx + 1);
                request_fetch(idx);
            }
            break;
        case APP_CMD_SET_WATCHLIST:
            apply_watchlist(c->text);
            break;
        case APP_CMD_SET_KEYS:
            apply_keys(c);
            break;
    }
}

/* Handle one (non-chord) button press, dispatched by the active view. */
static void handle_press(button_id_t id, int64_t refresh_us) {
    if (s_econ_mode) {                 /* calendar: KEY = prev week, BOOT = next */
        state_lock();
        s_econ_week += (id == BUTTON_USER) ? -1 : +1;
        state_unlock();
        render_econ();
        return;
    }

    if (id == BUTTON_USER) {           /* next ticker, same view */
        state_lock();
        int idx = (s_sym_index + 1) % (int)s_ticker_n;
        state_unlock();
        ESP_LOGI(TAG, "USER -> ticker %d/%u", idx + 1, (unsigned)s_ticker_n);
        apply_select(idx, refresh_us);
    } else {                           /* BOOT: next view, same ticker (no network) */
        state_lock();
        int page = (s_page + 1) % UI_STOCK_PAGE_COUNT;
        state_unlock();
        ESP_LOGI(TAG, "BOOT -> view %s (%d/%d)", kPageName[page], page + 1, UI_STOCK_PAGE_COUNT);
        apply_page(page);
    }
}

/*
 * StockTask — input + render only (higher priority), but it also performs the
 * economic-calendar fetch inline, so it carries a big stack (TLS + JSON). Button
 * presses are handled the instant they arrive:
 *   USER (GPIO18) -> next ticker; paints cached data now, fetches only if stale
 *   BOOT (GPIO0)  -> next view (home/chart/news/metrics), pure local page swap
 *   USER+BOOT     -> toggle the economic-calendar overlay (in it: prev/next week)
 * Between presses it wakes every HOME_TICK_SECONDS to keep the home clock and
 * battery chip live, and kicks a background refresh of the on-screen ticker on
 * the (slower) REFRESH cadence.
 */
static void StockTask(void *arg) {
    (void)arg;
    int refresh = CONFIG_STOCK_REFRESH_SECONDS; if (refresh < 1) refresh = 30;
    int64_t refresh_us = (int64_t)refresh * 1000000;
    s_refresh_us = refresh_us;          /* shared with the app's command handler (handle_cmd) */

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
        /* Wait on either a button press OR an app control command (or the idle timeout). The
         * chord detection below drains extra button events directly with a 0 timeout, which can
         * leave the queue set with a stale "ready" mark; a select that then yields no item is
         * treated as a spurious wake (continue), so the two paths never desync. */
        QueueSetMemberHandle_t member =
            xQueueSelectFromSet(s_queue_set, pdMS_TO_TICKS(HOME_TICK_SECONDS * 1000));

        if (member == s_btn_queue) {
            button_event_t ev;
            if (xQueueReceive(s_btn_queue, &ev, 0) != pdTRUE) continue;  /* spurious wake */
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
        } else if (member == s_cmd_queue) {
            app_cmd_t cmd;
            if (xQueueReceive(s_cmd_queue, &cmd, 0) == pdTRUE) {
                handle_cmd(&cmd);
            }
        } else {
            /* Idle timeout. Calendar view is static between presses — skip the home/network tick. */
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

    /* Per-ticker cache + per-worker scratch live in PSRAM (each ~1.6KB). Sized to the MAX
     * watchlist (not just the current length) so the app can replace the watchlist live
     * (user_app_set_watchlist) without reallocating this buffer under the fetch workers. */
    s_cache = (stock_data_t *)heap_caps_calloc(PROV_MAX_TICKERS, sizeof(stock_data_t),
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_cache) { ESP_LOGE(TAG, "cache alloc failed (%u slots)", (unsigned)PROV_MAX_TICKERS); return; }
    for (int i = 0; i < NUM_FETCHERS; i++) {
        s_scratch[i] = (stock_data_t *)heap_caps_malloc(sizeof(stock_data_t),
                                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_scratch[i]) { ESP_LOGE(TAG, "scratch alloc failed"); return; }
    }

    s_mtx        = xSemaphoreCreateMutex();
    s_fetch_wake = xSemaphoreCreateCounting(PROV_MAX_TICKERS + NUM_FETCHERS, 0);
    s_btn_queue  = xQueueCreate(16, sizeof(button_event_t));
    s_cmd_queue  = xQueueCreate(8, sizeof(app_cmd_t));
    /* A queue set lets StockTask block on buttons OR app commands in one wait. Both queues must
     * be empty when added to a set, so build the set before buttons_init starts posting. */
    s_queue_set  = xQueueCreateSet(16 + 8);
    xQueueAddToSet(s_btn_queue, s_queue_set);
    xQueueAddToSet(s_cmd_queue, s_queue_set);
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

/* ===========================================================================================
 * Companion-app control bridge (declared in user_app_control.h). These run on the HTTP server
 * task (components/stock_api): reads copy state under s_mtx; writes validate cheaply then post a
 * command for StockTask to apply via the same paths as the buttons. All are safe no-ops until
 * UserApp_TaskInit has created the queues.
 * =========================================================================================== */

void user_app_snapshot(stock_api_state_t *out) {
    memset(out, 0, sizeof(*out));
    strlcpy(out->model, STOCK_APP_MODEL, sizeof(out->model));
    strlcpy(out->fw, STOCK_APP_FW, sizeof(out->fw));
    int refresh = CONFIG_STOCK_REFRESH_SECONDS; if (refresh < 1) refresh = 30;
    out->refresh_seconds = refresh;
    if (!s_mtx) return;                 /* TaskInit has not run yet */

    state_lock();
    out->index     = s_sym_index;
    out->page      = s_page;
    out->econ_mode = s_econ_mode;
    out->econ_week = s_econ_week;
    out->keys.finnhub  = s_cfg.finnhub_key[0] != '\0';
    out->keys.fmp      = s_cfg.fmp_key[0] != '\0';
    out->keys.econ_url = s_cfg.econ_url[0] != '\0';
    out->env.valid         = s_last_env.env_valid;
    out->env.temp_c        = s_last_env.temp_c;
    out->env.humidity      = s_last_env.humidity;
    out->env.battery_valid = s_last_env.battery_valid;
    out->env.battery_v     = s_last_env.battery_v;
    out->env.battery_pct   = s_last_env.battery_pct;

    size_t n = s_ticker_n;
    if (n > STOCK_API_MAX_TICKERS) n = STOCK_API_MAX_TICKERS;
    out->ticker_count = n;
    int64_t now = esp_timer_get_time();
    for (size_t i = 0; i < n; i++) {
        stock_api_ticker_t *d = &out->tickers[i];
        strlcpy(d->symbol, s_cfg.tickers[i], sizeof(d->symbol));
        d->valid   = s_valid[i];
        d->price   = s_cache[i].quote.price;
        d->change  = s_cache[i].quote.change;
        d->percent = s_cache[i].quote.percent;
        d->age_sec = s_valid[i] ? (int)((now - s_time_us[i]) / 1000000) : -1;
    }
    state_unlock();
}

bool user_app_select_index(int index) {
    if (!s_cmd_queue) return false;
    state_lock();
    int n = (int)s_ticker_n;
    state_unlock();
    if (index < 0 || index >= n) return false;
    app_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = APP_CMD_SELECT_INDEX;
    c.ival = index;
    return xQueueSend(s_cmd_queue, &c, 0) == pdTRUE;
}

bool user_app_select_symbol(const char *symbol) {
    if (!s_cmd_queue || !symbol) return false;
    char norm[PROV_TICKER_MAX_LEN + 1];
    if (!prov_ticker_normalize(symbol, norm)) return false;
    int found = -1;
    state_lock();
    for (int i = 0; i < (int)s_ticker_n; i++) {
        if (strcmp(s_cfg.tickers[i], norm) == 0) { found = i; break; }
    }
    state_unlock();
    if (found < 0) return false;
    return user_app_select_index(found);
}

bool user_app_set_page(int page) {
    if (!s_cmd_queue) return false;
    if (page < 0 || page >= UI_STOCK_PAGE_COUNT) return false;
    app_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = APP_CMD_SET_PAGE;
    c.ival = page;
    return xQueueSend(s_cmd_queue, &c, 0) == pdTRUE;
}

void user_app_set_econ(bool mode, int week) {
    if (!s_cmd_queue) return;
    app_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = APP_CMD_SET_ECON;
    c.bval = mode;
    c.ival = week;
    xQueueSend(s_cmd_queue, &c, 0);
}

void user_app_refresh(bool all) {
    if (!s_cmd_queue) return;
    app_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = APP_CMD_REFRESH;
    c.bval = all;
    xQueueSend(s_cmd_queue, &c, 0);
}

int user_app_set_watchlist(const char *csv) {
    if (!s_cmd_queue || !csv) return 0;
    prov_config_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    size_t n = prov_tickers_parse(&tmp, csv);
    if (n == 0) return 0;
    app_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = APP_CMD_SET_WATCHLIST;
    prov_tickers_serialize(&tmp, c.text, sizeof(c.text));   /* normalized, deduped csv */
    if (xQueueSend(s_cmd_queue, &c, 0) != pdTRUE) return 0;
    return (int)n;
}

bool user_app_set_keys(const char *finnhub, const char *fmp, const char *econ_url) {
    if (!s_cmd_queue) return false;
    app_cmd_t c;
    memset(&c, 0, sizeof(c));
    c.kind = APP_CMD_SET_KEYS;
    // Each non-NULL argument is an updated field (empty string clears it -> Kconfig default);
    // NULL leaves that key unchanged.
    if (finnhub)  { c.set_finnhub = true; strlcpy(c.finnhub,  finnhub,  sizeof(c.finnhub)); }
    if (fmp)      { c.set_fmp     = true; strlcpy(c.fmp,      fmp,      sizeof(c.fmp)); }
    if (econ_url) { c.set_econ    = true; strlcpy(c.econ_url, econ_url, sizeof(c.econ_url)); }
    if (!c.set_finnhub && !c.set_fmp && !c.set_econ) return false;   // nothing to change
    return xQueueSend(s_cmd_queue, &c, 0) == pdTRUE;
}
