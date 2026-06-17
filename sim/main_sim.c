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
#include "ui_econ.h"
#include "stock_service.h"
#include "econ_service.h"
#include "econ_parse.h"

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
        .env_valid = true, .temp_c = 23.5f, .humidity = 45.0f,
        .battery_valid = true, .battery_pct = 84, .battery_v = 4.01f,
    };
    ui_stock_update_env(&env);

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
        econ_service_fetch(fmp, now, tz_off, 0, ECON_IMPACT_MEDIUM, &cal);
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
    ui_econ_show(true);
    int pages = econ_page_count(cal.valid ? cal.count : 0);
    for (int p = 0; p < pages && p < 4; p++) {        /* KEY pages through the week */
        ui_econ_set_calendar(&cal, p);
        run_refresh(16);
        if (p == 0) snprintf(path, sizeof(path), "%s/sim_econ.bmp", outdir);
        else        snprintf(path, sizeof(path), "%s/sim_econ_p%d.bmp", outdir, p + 1);
        write_mono_bmp(path);
        printf("wrote %s\n", path);
    }

    return 0;
}
