/*
 * LVGL desktop simulator for the stock monitor (headless -> PNG).
 *
 * Renders the real UI with REAL data: it runs the same stock_service +
 * parsers used on the device, fetching live over libcurl (this Mac has
 * internet; the board does not while I work). It then binarizes the RGB565
 * framebuffer with the device's exact rule (px < 0x7FFF ? black : white) and
 * writes one BMP per page so I can see what the reflective panel will show.
 *
 *   FINNHUB_KEY=... STOCK_SYMBOL=AAPL ./build/sim shots
 *
 * Yahoo rate-limits this host's IP, so when the intraday series can't be
 * fetched the chart falls back to a synthetic intraday walk (clearly logged)
 * purely so the chart renderer can be verified. On the board (different IP)
 * the live Yahoo path is used.
 */
#include "lvgl.h"
#include "ui_stock.h"
#include "ui_home.h"
#include "ui_econ.h"
#include "stock_service.h"
#include "econ_service.h"
#include "econ_parse.h"
#include "weather_service.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#define HOR 400
#define VER 300

static uint8_t  fb[HOR * VER * 2];
static uint16_t capture[HOR * VER];
static uint32_t g_tick = 0;

static uint32_t tick_cb(void) { return g_tick; }

static void flush_cb(lv_display_t *d, const lv_area_t *a, uint8_t *px) {
    int w = a->x2 - a->x1 + 1, h = a->y2 - a->y1 + 1;
    if (w == HOR && h == VER) memcpy(capture, px, sizeof(capture));
    lv_display_flush_ready(d);
}

static void run_refresh(int steps) {
    for (int i = 0; i < steps; i++) { g_tick += 16; lv_timer_handler(); }
}

/* capture[] (RGB565) -> 24bit BMP, device binarization rule. */
static void write_mono_bmp(const char *path) {
    int W = HOR, H = VER, rowsize = (W * 3 + 3) & ~3, datasize = rowsize * H;
    int filesize = 54 + datasize;
    uint8_t hdr[54] = {0};
    hdr[0]='B'; hdr[1]='M';
    hdr[2]=filesize; hdr[3]=filesize>>8; hdr[4]=filesize>>16; hdr[5]=filesize>>24;
    hdr[10]=54; hdr[14]=40;
    hdr[18]=W; hdr[19]=W>>8; hdr[22]=H; hdr[23]=H>>8;
    hdr[26]=1; hdr[28]=24;
    hdr[34]=datasize; hdr[35]=datasize>>8; hdr[36]=datasize>>16; hdr[37]=datasize>>24;
    FILE *f = fopen(path, "wb");
    if (!f) { printf("cannot open %s\n", path); return; }
    fwrite(hdr, 1, 54, f);
    uint8_t *row = calloc(1, rowsize);
    if (!row) { fclose(f); return; }
    for (int y = H - 1; y >= 0; y--) {
        for (int x = 0; x < W; x++) {
            uint16_t p = capture[y * W + x];
            uint8_t v = (p < 0x7FFF) ? 0 : 255;
            row[x*3]=v; row[x*3+1]=v; row[x*3+2]=v;
        }
        fwrite(row, 1, rowsize, f);
    }
    free(row); fclose(f);
}

/* Synthetic intraday walk so the chart renders when Yahoo blocks this IP. */
static void synth_series(stock_series_t *s, const char *sym, double anchor, double prev) {
    memset(s, 0, sizeof(*s));
    strncpy(s->symbol, sym, sizeof(s->symbol) - 1);
    strncpy(s->currency, "USD", sizeof(s->currency) - 1);
    s->prev_close = prev > 0 ? prev : anchor;
    s->gmtoffset  = -14400;            /* US Eastern (DST) -> realistic x-axis */
    s->t_start    = 1718371800;        /* ~09:30 exchange-local */
    int n = 54;                        /* partial session: line ends mid-day (~13:55) */
    double mn = 1e18, mx = -1e18;
    for (int i = 0; i < n; i++) {
        double v = anchor * (1.0 + 0.012 * sin(i * 0.28) + 0.005 * sin(i * 1.7 + 1.0)
                                 - 0.010 * (i / (double)n));
        s->close[i] = (float)v;
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    s->count    = n;
    s->t_end    = s->t_start + (int64_t)(n - 1) * 300;
    s->day_min  = mn;
    s->day_max  = mx;
    s->day_high = mx + (mx - mn) * 0.05;   /* true intraday H/L sit just outside closes */
    s->day_low  = mn - (mx - mn) * 0.05;
    s->valid    = true;
}

int main(int argc, char **argv) {
    const char *outdir = (argc > 1) ? argv[1] : "shots";
    const char *symbol = getenv("STOCK_SYMBOL"); if (!symbol || !*symbol) symbol = "AAPL";
    /* STOCK_SYMBOL may be a comma-separated watchlist (the firmware splits it into
     * tickers); the sim renders one symbol, so take the first token. */
    char sym1[16];
    {
        while (*symbol == ' ') symbol++;
        const char *comma = strchr(symbol, ',');
        size_t n = comma ? (size_t)(comma - symbol) : strlen(symbol);
        while (n > 0 && symbol[n - 1] == ' ') n--;
        if (n >= sizeof sym1) n = sizeof sym1 - 1;
        memcpy(sym1, symbol, n); sym1[n] = '\0';
        if (sym1[0]) symbol = sym1;
    }
    const char *key    = getenv("FINNHUB_KEY");   if (!key) key = "";

    lv_init();
    lv_tick_set_cb(tick_cb);
    lv_display_t *disp = lv_display_create(HOR, VER);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_buffers(disp, fb, NULL, sizeof(fb), LV_DISPLAY_RENDER_MODE_FULL);

    lv_obj_t *scr = lv_obj_create(NULL);
    lv_screen_load(scr);
    ui_stock_create(scr);

    printf("fetching %s (finnhub key %s) ...\n", symbol, *key ? "set" : "MISSING");
    stock_data_t data;
    int ok = stock_service_fetch(symbol, key, &data);
    printf("endpoints ok=%d/4  quote=%d series=%d metrics=%d news=%d\n",
           ok, data.quote.valid, data.series.valid, data.metrics.valid, data.news.valid);
    if (data.quote.valid)
        printf("  price=%.2f change=%+.2f (%+.2f%%)\n",
               data.quote.price, data.quote.change, data.quote.percent);
    if (data.news.valid && data.news.count > 0)
        printf("  top headline: %s\n", data.news.items[0].headline);

    if (!data.series.valid) {
        printf("  [chart] Yahoo unreachable from this host -> SYNTHETIC series\n");
        synth_series(&data.series, symbol,
                     data.quote.valid ? data.quote.price : 100.0,
                     data.quote.valid ? data.quote.prev_close : 100.0);
    }

    /* No live quote (no key / offline)? Use the reference-photo sample so the
     * home card renders with realistic content for design comparison. */
    if (!data.quote.valid) {
        memset(&data.quote, 0, sizeof(data.quote));
        snprintf(data.quote.symbol, sizeof(data.quote.symbol), "%s",
                 (getenv("FINNHUB_KEY") && *getenv("FINNHUB_KEY")) ? symbol : "PLTR");
        data.quote.price      = 23.29;
        data.quote.change     = 0.21;
        data.quote.percent    = 0.91;
        data.quote.prev_close = 23.08;
        data.quote.valid      = true;
        printf("  [home] using sample quote %s $%.2f (%+.2f%%)\n",
               data.quote.symbol, data.quote.price, data.quote.percent);
    }

    ui_stock_update(&data);

    /* Sample environment so temp/humidity/battery render on the home card. */
    ui_env_t env = {
        .env_valid = true, .temp_c = 22.0f, .humidity = 45.0f,
        .battery_valid = true, .battery_pct = 85, .battery_v = 4.01f,
    };
    ui_stock_update_env(&env);

    /* --- Sidebar watchlist: up to 3 REAL quotes -------------------------------
     * Exactly the on-device path: one quote per watchlist symbol via
     * stock_service. Live with FINNHUB_KEY (STOCK_SYMBOL may be a CSV list);
     * falls back to the reference rows when there's no key so the panel still
     * renders for design comparison. */
    home_ticker_t tks[HOME_TICKERS_MAX];
    int tn = 0;
    if (*key) {
        const char *wl = getenv("STOCK_SYMBOL"); if (!wl || !*wl) wl = "AAPL,TSLA,MSFT";
        const char *p = wl;
        while (*p && tn < HOME_TICKERS_MAX) {
            while (*p == ' ' || *p == ',') p++;
            const char *st = p;
            while (*p && *p != ',') p++;
            size_t len = (size_t)(p - st);
            while (len > 0 && st[len - 1] == ' ') len--;
            if (!len) continue;
            char s[16]; if (len >= sizeof s) len = sizeof s - 1;
            memcpy(s, st, len); s[len] = '\0';
            snprintf(tks[tn].symbol, sizeof tks[tn].symbol, "%s", s);
            stock_data_t q;
            if (tn == 0 && data.quote.valid) {           /* reuse the first fetch */
                tks[tn].price = data.quote.price; tks[tn].percent = data.quote.percent; tks[tn].valid = true;
            } else if (stock_service_fetch_quote(s, key, &q) && q.quote.valid) {
                tks[tn].price = q.quote.price; tks[tn].percent = q.quote.percent; tks[tn].valid = true;
            } else {
                tks[tn].price = 0; tks[tn].percent = 0; tks[tn].valid = false;
            }
            printf("[ticker %d] %-6s $%.2f (%+.2f%%) %s\n", tn, tks[tn].symbol,
                   tks[tn].price, tks[tn].percent, tks[tn].valid ? "" : "(no data)");
            tn++;
        }
    }
    if (tn == 0) {
        home_ticker_t sample[3] = {
            { "AAPL", 173.50, +1.25, true },
            { "TSLA", 212.10, -0.50, true },
            { "MSFT", 330.11, +0.80, true },
        };
        memcpy(tks, sample, sizeof sample); tn = 3;
        printf("[tickers] sample fallback (no FINNHUB_KEY)\n");
    }
    ui_home_set_tickers(tks, tn);

    /* --- Weather: REAL via Open-Meteo (keyless) when LOCATION is set ------------
     * Geocode the typed place to a coordinate, then fetch current + 7-day. This
     * is the exact on-device WeatherTask path. Sample fallback otherwise. */
    bool wx_done = false;
    const char *loc = getenv("LOCATION");
    if (loc && *loc) {
        geo_loc_t g;
        weather_t w;
        if (weather_service_geocode(loc, &g) && weather_service_fetch(g.lat, g.lon, &w) && w.valid) {
            char city[64];
            if (g.country[0]) snprintf(city, sizeof city, "%s, %s", g.name, g.country);
            else              snprintf(city, sizeof city, "%s", g.name);
            if (w.now_valid) ui_home_set_weather((home_wx_t)w.now_wx, w.now_temp_c, city);
            home_forecast_t fcr[HOME_FORECAST_MAX];
            int fn = w.day_count; if (fn > HOME_FORECAST_MAX) fn = HOME_FORECAST_MAX;
            for (int i = 0; i < fn; i++) {
                snprintf(fcr[i].dow, sizeof fcr[i].dow, "%s", w.days[i].dow);
                fcr[i].wx = (home_wx_t)w.days[i].wx;
                fcr[i].lo = w.days[i].lo; fcr[i].hi = w.days[i].hi;
            }
            ui_home_set_forecast(fcr, fn);
            printf("[weather] %s -> %s  %d°C, %d-day forecast\n", loc, city, w.now_temp_c, fn);
            wx_done = true;
        }
        if (!wx_done) printf("[weather] lookup failed for '%s' -> sample\n", loc);
    }
    if (!wx_done) {
        ui_home_set_weather(HOME_WX_SUN, 24, "Seoul, KR");
        home_forecast_t fc[7] = {
            { "FRI", HOME_WX_PARTLY, 15, 22 },
            { "SAT", HOME_WX_SUN,    16, 24 },
            { "SUN", HOME_WX_CLOUD,  14, 20 },
            { "MON", HOME_WX_RAIN,   12, 17 },
            { "TUE", HOME_WX_SUN,    14, 21 },
            { "WED", HOME_WX_PARTLY, 15, 22 },
            { "THU", HOME_WX_SUN,    16, 24 },
        };
        ui_home_set_forecast(fc, 7);
    }

    /* --- Econ: the 3 NEAREST upcoming high-impact events -----------------------
     * With FMP_KEY set, fetch the real HIGH-only calendar (this week, then next
     * week if nothing high is still upcoming) and show the next 3 — exactly what
     * EconTask + the home tick do on-device. Sample fallback otherwise. */
    time_t now_home = time(NULL);
    long   tz_home  = econ_local_tz_off(now_home);
    const char *home_fmp = getenv("FMP_KEY"); if (!home_fmp) home_fmp = "";
    econ_calendar_t home_cal;
    const econ_event_t *picked[HOME_ECON_MAX];
    int en = 0;
    if (*home_fmp) {
        econ_service_fetch(home_fmp, NULL, now_home, tz_home, 0, ECON_IMPACT_HIGH, &home_cal);
        if (home_cal.valid && econ_next_after(&home_cal, (int64_t)now_home) < 0)
            econ_service_fetch(home_fmp, NULL, now_home, tz_home, +1, ECON_IMPACT_HIGH, &home_cal);
        if (home_cal.valid)
            en = econ_collect_upcoming(&home_cal, (int64_t)now_home, picked, HOME_ECON_MAX);
        printf("[home econ] valid=%d count=%d upcoming=%d %s\n", home_cal.valid,
               home_cal.count, en, home_cal.valid ? "" : home_cal.error);
    }
    if (en > 0) {
        econ_event_t evs[HOME_ECON_MAX];
        char         wb[HOME_ECON_MAX][16];
        const char  *wp[HOME_ECON_MAX];
        for (int i = 0; i < en; i++) {
            evs[i] = *picked[i];
            econ_when_label(evs[i].ts, now_home, tz_home, wb[i], sizeof wb[i]);
            wp[i] = wb[i];
            printf("[home econ %d] %s  %s  (%s)\n", i, wb[i], evs[i].event, evs[i].country);
        }
        ui_stock_update_econ(evs, wp, en);
    } else {
        /* Three sample events spread over the next couple of days. */
        static const struct { const char *cc, *name, *est, *act, *prev; int hrs; } S[3] = {
            { "US", "US Core CPI YoY",   "3.2%",  "3.1%", "3.0%",  3 },
            { "US", "Fed Rate Decision", "4.50%", "",     "4.50%", 27 },
            { "JP", "BoJ Rate Decision", "0.75%", "",     "0.50%", 50 },
        };
        econ_event_t evs[3];
        char         wb[3][16];
        const char  *wp[3];
        for (int i = 0; i < 3; i++) {
            memset(&evs[i], 0, sizeof evs[i]);
            snprintf(evs[i].country,  sizeof evs[i].country,  "%s", S[i].cc);
            snprintf(evs[i].event,    sizeof evs[i].event,    "%s", S[i].name);
            snprintf(evs[i].estimate, sizeof evs[i].estimate, "%s", S[i].est);
            snprintf(evs[i].actual,   sizeof evs[i].actual,   "%s", S[i].act);
            snprintf(evs[i].previous, sizeof evs[i].previous, "%s", S[i].prev);
            evs[i].impact = ECON_IMPACT_HIGH;
            evs[i].ts = (int64_t)now_home + S[i].hrs * 3600;
            econ_when_label(evs[i].ts, now_home, tz_home, wb[i], sizeof wb[i]);
            wp[i] = wb[i];
        }
        ui_stock_update_econ(evs, wp, 3);
        printf("[home econ] sample fallback (no FMP_KEY or nothing upcoming)\n");
    }

    char path[640];
    for (int p = 0; p < UI_STOCK_PAGE_COUNT; p++) {
        ui_stock_show_page(p);
        run_refresh(16);
        snprintf(path, sizeof(path), "%s/sim_page%d.bmp", outdir, p);
        write_mono_bmp(path);
        printf("wrote %s\n", path);
    }

    /* Economic-calendar overlay (KEY+BOOT on the device). Renders live data when
     * FMP_KEY is set, otherwise a canned week through the real parse+render path
     * so the layout can be checked offline. */
    ui_econ_create(scr);
    const char *fmp = getenv("FMP_KEY"); if (!fmp) fmp = "";
    time_t now = time(NULL);
    long tz_off = econ_local_tz_off(now);

    econ_calendar_t cal;
    if (*fmp) {
        econ_service_fetch(fmp, NULL, now, tz_off, 0, ECON_IMPACT_MEDIUM, &cal);
        printf("[econ] week %s valid=%d count=%d/%d %s\n", cal.week_label,
               cal.valid, cal.count, cal.total_matched, cal.valid ? "" : cal.error);
    } else {
        static const char *SAMPLE =
            "[{\"date\":\"2026-06-15 12:30:00\",\"country\":\"US\",\"event\":\"CPI YoY\","
            "\"previous\":3.1,\"estimate\":3.2,\"actual\":3.4,\"impact\":\"High\"},"
            "{\"date\":\"2026-06-16 02:00:00\",\"country\":\"JP\",\"event\":\"BoJ Interest Rate Decision\","
            "\"previous\":0.75,\"estimate\":0.75,\"actual\":1.0,\"impact\":\"High\"},"
            "{\"date\":\"2026-06-17 14:00:00\",\"country\":\"US\",\"event\":\"Fed Interest Rate Decision\","
            "\"previous\":4.5,\"estimate\":4.5,\"actual\":null,\"impact\":\"High\"},"
            "{\"date\":\"2026-06-18 08:30:00\",\"country\":\"EU\",\"event\":\"ECB Press Conference\","
            "\"previous\":null,\"estimate\":null,\"actual\":null,\"impact\":\"High\"},"
            "{\"date\":\"2026-06-19 06:00:00\",\"country\":\"GB\",\"event\":\"GDP Growth Rate QoQ\","
            "\"previous\":0.3,\"estimate\":0.4,\"actual\":null,\"impact\":\"High\"}]";
        econ_parse_calendar(SAMPLE, tz_off, ECON_IMPACT_HIGH, &cal);
        snprintf(cal.week_label, sizeof(cal.week_label), "06-15 ~ 06-21");
        printf("[econ] sample week (no FMP_KEY) count=%d\n", cal.count);
    }
    ui_econ_set_calendar(&cal);
    ui_econ_show(true);
    run_refresh(16);
    snprintf(path, sizeof(path), "%s/sim_econ.bmp", outdir);
    write_mono_bmp(path);
    printf("wrote %s\n", path);

    return 0;
}
