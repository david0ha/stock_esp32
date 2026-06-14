/*
 * ui_stock.c — LVGL stock-monitor UI tuned for a 400x300 monochrome panel.
 *
 * Design notes for the reflective B/W display:
 *   - Pure black on white only; the panel binarizes at mid-gray so there are
 *     no usable grays. Hierarchy comes from font size and a filled-black badge.
 *   - Chart values are stored in integer cents (price*100) because lv_chart
 *     works in int32.
 */
#include "ui_stock.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define SCR_W      400
#define SCR_H      300
#define BAR_H      64
#define PAGE_Y     (BAR_H + 4)
#define DOTS_H     16
#define PAGE_H     (SCR_H - PAGE_Y - DOTS_H)
#define NEWS_ROWS  STOCK_NEWS_MAX
#define METRIC_N   6
#define DOT_SIZE   10
#define DOT_GAP    8
#define CHART_YPAD_FACTOR 0.08   /* soften chart Y-range by 8% of the day span */

static const lv_color_t BLACK = LV_COLOR_MAKE(0, 0, 0);
static const lv_color_t WHITE = LV_COLOR_MAKE(0xff, 0xff, 0xff);

static struct {
    lv_obj_t          *lbl_symbol;
    lv_obj_t          *lbl_price;
    lv_obj_t          *lbl_change;     /* black pill badge */
    lv_obj_t          *page[UI_STOCK_PAGE_COUNT];
    lv_obj_t          *chart;
    lv_chart_series_t *series;
    lv_obj_t          *lbl_chart_title;  /* "{date}  5m" */
    lv_obj_t          *lbl_chart_hl;     /* "H .. L .."  */
    lv_obj_t          *lbl_t_start;      /* x-axis: first time  */
    lv_obj_t          *lbl_t_mid;        /* x-axis: middle time */
    lv_obj_t          *lbl_t_end;        /* x-axis: last (~now) time */
    lv_obj_t          *lbl_chart_empty;
    lv_obj_t          *news_row[NEWS_ROWS];
    lv_obj_t          *metric_name[METRIC_N];
    lv_obj_t          *metric_val[METRIC_N];
    lv_obj_t          *dot[UI_STOCK_PAGE_COUNT];
    int                cur_page;
} S;

static const char *METRIC_LABELS[METRIC_N] = {
    "P/E", "EPS", "MKT CAP", "DIV YIELD", "52W LOW", "52W HIGH"
};

/* ---- helpers ------------------------------------------------------------ */

static lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, BLACK, 0);
    return l;
}

/* A borderless, padding-free, transparent container (clean layout box). */
static lv_obj_t *mk_panel(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *p = lv_obj_create(parent);
    lv_obj_set_pos(p, x, y);
    lv_obj_set_size(p, w, h);
    lv_obj_set_style_bg_opa(p, 0, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_style_pad_all(p, 0, 0);
    lv_obj_set_style_radius(p, 0, 0);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    return p;
}

/* market cap arrives in millions of USD */
static void fmt_market_cap(char *buf, size_t n, double cap_m) {
    if (cap_m >= 1e6)      snprintf(buf, n, "$%.2fT", cap_m / 1e6);
    else if (cap_m >= 1e3) snprintf(buf, n, "$%.1fB", cap_m / 1e3);
    else if (cap_m > 0)    snprintf(buf, n, "$%.0fM", cap_m);
    else                   snprintf(buf, n, "--");
}

static void fmt_num(char *buf, size_t n, double v, const char *suffix) {
    if (v != 0.0) snprintf(buf, n, "%.2f%s", v, suffix);
    else          snprintf(buf, n, "--");
}

/* Epoch (+exchange offset) -> exchange-local strings. We add the offset and
 * read it back with gmtime_r so the labels show the market's local clock
 * (e.g. 09:30-16:00 ET) regardless of the device timezone. */
static void fmt_hhmm(char *buf, size_t n, int64_t epoch, int32_t off) {
    time_t t = (time_t)(epoch + off);
    struct tm tm;
    if (gmtime_r(&t, &tm)) strftime(buf, n, "%H:%M", &tm);
    else if (n)           buf[0] = '\0';   /* out-of-range epoch -> blank, not garbage */
}

static void fmt_date(char *buf, size_t n, int64_t epoch, int32_t off) {
    time_t t = (time_t)(epoch + off);
    struct tm tm;
    if (gmtime_r(&t, &tm)) strftime(buf, n, "%b %d", &tm);   /* e.g. "Jun 14" */
    else if (n)           buf[0] = '\0';
}

/* ---- top bar ------------------------------------------------------------ */

static void build_top_bar(lv_obj_t *parent) {
    S.lbl_symbol = mk_label(parent, &lv_font_montserrat_28);
    lv_label_set_text(S.lbl_symbol, "----");
    lv_obj_align(S.lbl_symbol, LV_ALIGN_TOP_LEFT, 10, 6);

    S.lbl_price = mk_label(parent, &lv_font_montserrat_28);
    lv_label_set_text(S.lbl_price, "--");
    lv_obj_align(S.lbl_price, LV_ALIGN_TOP_RIGHT, -10, 6);

    /* change-% as a filled black pill with white text — the one element that
     * must pop, achieved without color. */
    S.lbl_change = lv_label_create(parent);
    lv_obj_set_style_text_font(S.lbl_change, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(S.lbl_change, WHITE, 0);
    lv_obj_set_style_bg_opa(S.lbl_change, 255, 0);
    lv_obj_set_style_bg_color(S.lbl_change, BLACK, 0);
    lv_obj_set_style_radius(S.lbl_change, 8, 0);
    lv_obj_set_style_pad_hor(S.lbl_change, 8, 0);
    lv_obj_set_style_pad_ver(S.lbl_change, 3, 0);
    lv_label_set_text(S.lbl_change, "--");
    lv_obj_align(S.lbl_change, LV_ALIGN_TOP_RIGHT, -10, 40);

    /* divider under the bar */
    static lv_point_precise_t line_pts[2];
    line_pts[0].x = 0;      line_pts[0].y = BAR_H;
    line_pts[1].x = SCR_W;  line_pts[1].y = BAR_H;
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, line_pts, 2);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_color(line, BLACK, 0);
}

/* ---- page 0: chart ------------------------------------------------------ */

#define CHART_TOP   18                      /* below title row              */
#define CHART_XAXIS 18                      /* x-axis label strip at bottom */

static void build_chart_page(lv_obj_t *page) {
    S.lbl_chart_title = mk_label(page, &lv_font_montserrat_14);
    lv_label_set_text(S.lbl_chart_title, "INTRADAY 5m");
    lv_obj_align(S.lbl_chart_title, LV_ALIGN_TOP_LEFT, 4, 0);

    S.lbl_chart_hl = mk_label(page, &lv_font_montserrat_14);
    lv_label_set_text(S.lbl_chart_hl, "");
    lv_obj_align(S.lbl_chart_hl, LV_ALIGN_TOP_RIGHT, -4, 0);

    S.chart = lv_chart_create(page);
    lv_obj_set_size(S.chart, SCR_W - 24, PAGE_H - CHART_TOP - CHART_XAXIS - 2);
    lv_obj_align(S.chart, LV_ALIGN_TOP_MID, 0, CHART_TOP);
    lv_chart_set_type(S.chart, LV_CHART_TYPE_LINE);
    lv_chart_set_div_line_count(S.chart, 0, 0);
    lv_obj_set_style_bg_opa(S.chart, 0, 0);
    lv_obj_set_style_border_width(S.chart, 2, 0);
    lv_obj_set_style_border_color(S.chart, BLACK, 0);
    lv_obj_set_style_radius(S.chart, 0, 0);
    lv_obj_set_style_pad_all(S.chart, 0, 0);
    /* thick black line, no point markers */
    lv_obj_set_style_line_width(S.chart, 3, LV_PART_ITEMS);
    lv_obj_set_style_size(S.chart, 0, 0, LV_PART_INDICATOR);
    S.series = lv_chart_add_series(S.chart, BLACK, LV_CHART_AXIS_PRIMARY_Y);

    /* x-axis time labels aligned to the chart's left / centre / right */
    S.lbl_t_start = mk_label(page, &lv_font_montserrat_14);
    lv_obj_align(S.lbl_t_start, LV_ALIGN_BOTTOM_LEFT, 10, 0);
    S.lbl_t_mid = mk_label(page, &lv_font_montserrat_14);
    lv_obj_align(S.lbl_t_mid, LV_ALIGN_BOTTOM_MID, 0, 0);
    S.lbl_t_end = mk_label(page, &lv_font_montserrat_14);
    lv_obj_align(S.lbl_t_end, LV_ALIGN_BOTTOM_RIGHT, -10, 0);
    lv_label_set_text(S.lbl_t_start, "");
    lv_label_set_text(S.lbl_t_mid, "");
    lv_label_set_text(S.lbl_t_end, "");

    S.lbl_chart_empty = mk_label(page, &lv_font_montserrat_20);
    lv_label_set_text(S.lbl_chart_empty, "no chart data");
    lv_obj_align(S.lbl_chart_empty, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(S.lbl_chart_empty, LV_OBJ_FLAG_HIDDEN);
}

static void update_chart(const stock_series_t *s) {
    int n = s->count;
    if (n > STOCK_CANDLE_MAX) n = STOCK_CANDLE_MAX;   /* defensive: never index past close[] */
    if (!s->valid || n < 2) {
        lv_obj_add_flag(S.chart, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(S.lbl_chart_empty, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(S.lbl_chart_title, "INTRADAY 5m");  /* drop stale date */
        lv_label_set_text(S.lbl_chart_hl, "");
        lv_label_set_text(S.lbl_t_start, "");
        lv_label_set_text(S.lbl_t_mid, "");
        lv_label_set_text(S.lbl_t_end, "");
        return;
    }
    lv_obj_clear_flag(S.chart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(S.lbl_chart_empty, LV_OBJ_FLAG_HIDDEN);

    double pad = (s->day_max - s->day_min) * CHART_YPAD_FACTOR;
    if (pad <= 0) pad = s->day_max * 0.01 + 1.0;
    int32_t lo = (int32_t)((s->day_min - pad) * 100.0);
    int32_t hi = (int32_t)((s->day_max + pad) * 100.0);
    lv_chart_set_range(S.chart, LV_CHART_AXIS_PRIMARY_Y, lo, hi);

    lv_chart_set_point_count(S.chart, n);
    for (int i = 0; i < n; i++)
        lv_chart_set_value_by_id(S.chart, S.series, i, (int32_t)(s->close[i] * 100.0));
    lv_chart_refresh(S.chart);

    char buf[28];
    snprintf(buf, sizeof(buf), "H %.2f  L %.2f", s->day_high, s->day_low);
    lv_label_set_text(S.lbl_chart_hl, buf);

    /* title shows the trading date; x-axis shows start / middle / last(~now) */
    char date[16];
    fmt_date(date, sizeof(date), s->t_end, s->gmtoffset);
    snprintf(buf, sizeof(buf), "%s  5m", date);
    lv_label_set_text(S.lbl_chart_title, buf);

    char hhmm[8];
    fmt_hhmm(hhmm, sizeof(hhmm), s->t_start, s->gmtoffset);
    lv_label_set_text(S.lbl_t_start, hhmm);
    fmt_hhmm(hhmm, sizeof(hhmm), s->t_start + (s->t_end - s->t_start) / 2, s->gmtoffset);
    lv_label_set_text(S.lbl_t_mid, hhmm);
    fmt_hhmm(hhmm, sizeof(hhmm), s->t_end, s->gmtoffset);
    lv_label_set_text(S.lbl_t_end, hhmm);
}

/* ---- page 1: news ------------------------------------------------------- */

static void build_news_page(lv_obj_t *page) {
    lv_obj_t *title = mk_label(page, &lv_font_montserrat_14);
    lv_label_set_text(title, "NEWS");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

    int row_h = (PAGE_H - 24) / NEWS_ROWS;
    for (int i = 0; i < NEWS_ROWS; i++) {
        lv_obj_t *l = mk_label(page, &lv_font_montserrat_16);
        /* fixed one-line box so LONG_DOT truncates instead of wrapping */
        lv_obj_set_size(l, SCR_W - 16, 22);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
        lv_label_set_text(l, "");
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 6, 24 + i * row_h);
        S.news_row[i] = l;
    }
}

static void update_news(const stock_news_t *n) {
    for (int i = 0; i < NEWS_ROWS; i++) {
        if (n->valid && i < n->count) {
            char line[STOCK_HEADLINE_MAXLEN + 8];
            snprintf(line, sizeof(line), LV_SYMBOL_RIGHT "  %s", n->items[i].headline);
            lv_label_set_text(S.news_row[i], line);
        } else {
            bool empty = !n->valid || n->count == 0;
            lv_label_set_text(S.news_row[i], (i == 0 && empty) ? "no news" : "");
        }
    }
}

/* ---- page 2: metrics ---------------------------------------------------- */

static void build_metrics_page(lv_obj_t *page) {
    lv_obj_t *title = mk_label(page, &lv_font_montserrat_14);
    lv_label_set_text(title, "KEY METRICS");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 4, 0);

    const int col_x[2] = {8, SCR_W / 2 + 4};
    const int top = 26;
    const int cell_h = (PAGE_H - top - 6) / 3;
    for (int i = 0; i < METRIC_N; i++) {
        int cx = col_x[i % 2];
        int cy = top + (i / 2) * cell_h;

        lv_obj_t *name = mk_label(page, &lv_font_montserrat_14);
        lv_label_set_text(name, METRIC_LABELS[i]);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, cx, cy);
        S.metric_name[i] = name;

        lv_obj_t *val = mk_label(page, &lv_font_montserrat_20);
        lv_label_set_text(val, "--");
        lv_obj_align(val, LV_ALIGN_TOP_LEFT, cx, cy + 16);
        S.metric_val[i] = val;
    }
}

static void update_metrics(const stock_metrics_t *m) {
    char buf[24];
    if (!m->valid) {
        for (int i = 0; i < METRIC_N; i++) lv_label_set_text(S.metric_val[i], "--");
        return;
    }
    fmt_num(buf, sizeof(buf), m->pe_ttm, "");        lv_label_set_text(S.metric_val[0], buf);
    fmt_num(buf, sizeof(buf), m->eps_ttm, "");       lv_label_set_text(S.metric_val[1], buf);
    fmt_market_cap(buf, sizeof(buf), m->market_cap); lv_label_set_text(S.metric_val[2], buf);
    /* dividend yield: 0 is a real value (no dividend), not "missing" */
    snprintf(buf, sizeof(buf), "%.2f%%", m->div_yield); lv_label_set_text(S.metric_val[3], buf);
    fmt_num(buf, sizeof(buf), m->week52_low, "");    lv_label_set_text(S.metric_val[4], buf);
    fmt_num(buf, sizeof(buf), m->week52_high, "");   lv_label_set_text(S.metric_val[5], buf);
}

/* ---- page dots ---------------------------------------------------------- */

static void build_dots(lv_obj_t *parent) {
    int spacing = DOT_SIZE + DOT_GAP;
    int total_w = UI_STOCK_PAGE_COUNT * DOT_SIZE + (UI_STOCK_PAGE_COUNT - 1) * DOT_GAP;
    int x0 = (SCR_W - total_w) / 2;
    for (int i = 0; i < UI_STOCK_PAGE_COUNT; i++) {
        lv_obj_t *d = lv_obj_create(parent);
        lv_obj_set_size(d, DOT_SIZE, DOT_SIZE);
        lv_obj_set_pos(d, x0 + i * spacing, SCR_H - 13);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(d, 2, 0);
        lv_obj_set_style_border_color(d, BLACK, 0);
        lv_obj_set_style_bg_color(d, BLACK, 0);   /* fill (when active) is black */
        lv_obj_set_style_bg_opa(d, 0, 0);
        lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
        S.dot[i] = d;
    }
}

static void refresh_dots(void) {
    for (int i = 0; i < UI_STOCK_PAGE_COUNT; i++)
        lv_obj_set_style_bg_opa(S.dot[i], i == S.cur_page ? 255 : 0, 0);
}

/* ---- public API --------------------------------------------------------- */

void ui_stock_create(lv_obj_t *parent) {
    memset(&S, 0, sizeof(S));
    lv_obj_set_style_bg_opa(parent, 255, 0);
    lv_obj_set_style_bg_color(parent, WHITE, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    build_top_bar(parent);

    for (int i = 0; i < UI_STOCK_PAGE_COUNT; i++) {
        S.page[i] = mk_panel(parent, 0, PAGE_Y, SCR_W, PAGE_H);
        if (i != 0) lv_obj_add_flag(S.page[i], LV_OBJ_FLAG_HIDDEN);
    }
    build_chart_page(S.page[0]);
    build_news_page(S.page[1]);
    build_metrics_page(S.page[2]);
    build_dots(parent);

    S.cur_page = 0;
    refresh_dots();
}

void ui_stock_update(const stock_data_t *d) {
    const stock_quote_t *q = &d->quote;
    const char *sym = q->valid && q->symbol[0]   ? q->symbol
                    : d->series.valid             ? d->series.symbol
                    : "----";
    lv_label_set_text(S.lbl_symbol, sym);

    if (q->valid) {
        /* Use snprintf (full float support) not lv_label_set_text_fmt — LVGL's
         * built-in printf has floats disabled in the firmware config, which
         * otherwise renders "%.2f" literally as "f". */
        char buf[40];
        snprintf(buf, sizeof(buf), "%.2f", q->price);
        lv_label_set_text(S.lbl_price, buf);
        const char *arrow = q->change >= 0 ? LV_SYMBOL_UP : LV_SYMBOL_DOWN;
        snprintf(buf, sizeof(buf), "%s %+.2f (%+.2f%%)", arrow, q->change, q->percent);
        lv_label_set_text(S.lbl_change, buf);
    } else {
        lv_label_set_text(S.lbl_price, "--");
        lv_label_set_text(S.lbl_change, "no quote");
    }
    /* re-align right-anchored bar items after text width changes */
    lv_obj_align(S.lbl_price, LV_ALIGN_TOP_RIGHT, -10, 6);
    lv_obj_align(S.lbl_change, LV_ALIGN_TOP_RIGHT, -10, 40);

    update_chart(&d->series);
    update_news(&d->news);
    update_metrics(&d->metrics);
}

void ui_stock_show_page(int index) {
    if (index < 0 || index >= UI_STOCK_PAGE_COUNT) return;
    lv_obj_add_flag(S.page[S.cur_page], LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(S.page[index], LV_OBJ_FLAG_HIDDEN);
    S.cur_page = index;
    refresh_dots();
}
