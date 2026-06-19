/*
 * ui_home.c — desk-display dashboard for the 400x300 reflective mono panel.
 *
 * Layout (modelled on the reference desk render):
 *
 *   +----------+--------------------------------------------+
 *   |  AAPL    |  14:30                         [wx]  24°C   |   header
 *   | $173.50  |  Thu, Oct 24                       Seoul,KR |
 *   | ^ +1.25% |--------------------------------------------|
 *   |  TSLA    |  FRI  SAT  SUN  MON  TUE  WED  THU          |   forecast
 *   | $212.10  | [wx] [wx] [wx] [wx] [wx] [wx] [wx]          |
 *   | v -0.50% | 15/22 ...                                   |
 *   |  MSFT    |--------------------------------------------|
 *   | $330.11  | (mega) [21:30] US Core CPI YoY             |   econ
 *   | ^ +0.80% | Expected 3.2%  |  Actual 3.1%              |
 *   | Next >   |---------(drop)45% | (thermo)22 | (batt)85%-|   status
 *   +----------+--------------------------------------------+
 *
 * The sidebar ticker bar is bold; everything on the right is lighter Montserrat
 * for an airy, modern feel. Weather/alert/status glyphs are vector icons drawn
 * by ui_icons.c so there are no image assets and it binarizes cleanly.
 */
#include "ui_home.h"
#include "ui_fonts.h"
#include "ui_icons.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define W   400
#define H   300
#define SB  118          /* sidebar width                         */
#define MX  (SB + 16)    /* main-area left content edge           */
#define RX  (W - 14)     /* main-area right content edge          */

/* horizontal band dividers */
#define DIV_HEADER   95
#define DIV_FORECAST 188
#define DIV_ECON     246

#define F_CLOCK (&ui_font_mont_bold_46)      /* hero clock            */
#define F_SYM   (&ui_font_mont_bold_20)      /* sidebar symbol        */
#define F_BIG   (&lv_font_montserrat_20)     /* weather temp / status */
#define F_MED   (&lv_font_montserrat_16)     /* price, date           */
#define F_SML   (&lv_font_montserrat_14)     /* change, city, captions*/
#define F_TINY  (&lv_font_montserrat_12)     /* dense econ rows        */

static const lv_color_t BLACK = LV_COLOR_MAKE(0, 0, 0);
static const lv_color_t WHITE = LV_COLOR_MAKE(0xff, 0xff, 0xff);

static struct {
    lv_obj_t *page;
    /* header */
    lv_obj_t *clock, *date, *wx_icon, *wx_temp, *city;
    /* sidebar tickers */
    struct { lv_obj_t *sym, *price, *chg, *arrow; } tk[HOME_TICKERS_MAX];
    /* forecast columns */
    struct { lv_obj_t *dow, *icon, *temp; } fc[HOME_FORECAST_MAX];
    home_wx_t fc_wx[HOME_FORECAST_MAX];
    /* econ: nearest upcoming events, one full-width row each (event name on the
     * left, est/act on the right). */
    struct { lv_obj_t *name, *val; } ec[HOME_ECON_MAX];
    /* status */
    lv_obj_t *humid, *temp_in, *batt, *batt_icon;
} S;

/* ---- small builders ----------------------------------------------------- */

static lv_obj_t *lbl(lv_obj_t *p, const lv_font_t *f) {
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, BLACK, 0);
    return l;
}

/* LVGL keeps the points array BY REFERENCE (no copy), so each line's two points
 * must live for the object's lifetime — hence the static pools. They are sized
 * to the real divider count and guarded (never wrap) so an extra divider is
 * dropped rather than silently overwriting a live line's geometry. */
static void hline(lv_obj_t *p, int x1, int x2, int y, int w, bool dotted) {
    static lv_point_precise_t pts[8][2];
    static int n = 0;
    if (n >= (int)(sizeof pts / sizeof pts[0])) return;
    int i = n++;
    pts[i][0].x = x1; pts[i][0].y = y;
    pts[i][1].x = x2; pts[i][1].y = y;
    lv_obj_t *l = lv_line_create(p);
    lv_line_set_points(l, pts[i], 2);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_color(l, BLACK, 0);
    if (dotted) {
        lv_obj_set_style_line_dash_width(l, 1, 0);
        lv_obj_set_style_line_dash_gap(l, 4, 0);
    }
}

static void vline(lv_obj_t *p, int x, int y1, int y2, int w, bool dotted) {
    static lv_point_precise_t pts[4][2];
    static int n = 0;
    if (n >= (int)(sizeof pts / sizeof pts[0])) return;
    int i = n++;
    pts[i][0].x = x; pts[i][0].y = y1;
    pts[i][1].x = x; pts[i][1].y = y2;
    lv_obj_t *l = lv_line_create(p);
    lv_line_set_points(l, pts[i], 2);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_color(l, BLACK, 0);
    if (dotted) {
        lv_obj_set_style_line_dash_width(l, 1, 0);
        lv_obj_set_style_line_dash_gap(l, 4, 0);
    }
}

static ui_icon_t wx_to_icon(home_wx_t wx) {
    return wx == HOME_WX_SUN    ? ICON_SUN
         : wx == HOME_WX_PARTLY ? ICON_PARTLY
         : wx == HOME_WX_CLOUD  ? ICON_CLOUD
                                : ICON_RAIN;
}

static lv_obj_t *wx_icon(lv_obj_t *p, home_wx_t wx, int size) {
    return ui_icon(p, wx_to_icon(wx), size, 0);
}

/* ---- sidebar ------------------------------------------------------------ */

static const int TK_TOP[HOME_TICKERS_MAX] = { 14, 104, 194 };

/* clamp a label to a max width and dot-truncate so over-long text can never
 * spill past the sidebar divider into the clock/forecast area. */
static void clamp(lv_obj_t *l, int w) {
    lv_obj_set_width(l, w);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
}

static void build_sidebar(lv_obj_t *p) {
    for (int i = 0; i < HOME_TICKERS_MAX; i++) {
        int y = TK_TOP[i];
        S.tk[i].sym = lbl(p, F_SYM);
        lv_label_set_text(S.tk[i].sym, "----");
        clamp(S.tk[i].sym, SB - 26);
        lv_obj_align(S.tk[i].sym, LV_ALIGN_TOP_LEFT, 14, y);

        S.tk[i].price = lbl(p, F_MED);
        lv_label_set_text(S.tk[i].price, "");
        clamp(S.tk[i].price, SB - 26);
        lv_obj_align(S.tk[i].price, LV_ALIGN_TOP_LEFT, 14, y + 26);

        S.tk[i].arrow = NULL;     /* created lazily by set_tickers */
        S.tk[i].chg = lbl(p, F_SML);
        lv_label_set_text(S.tk[i].chg, "");
        clamp(S.tk[i].chg, SB - 40);
        lv_obj_align(S.tk[i].chg, LV_ALIGN_TOP_LEFT, 28, y + 48);
    }
    hline(p, 12, SB - 10, 94,  1, true);
    hline(p, 12, SB - 10, 184, 1, true);

    /* "Next >" affordance */
    lv_obj_t *next = lbl(p, F_SML);
    lv_label_set_text(next, "Next");
    lv_obj_align(next, LV_ALIGN_TOP_LEFT, 14, 278);
    lv_obj_t *chev = ui_icon(p, ICON_CHEVRON, 16, 0);
    lv_obj_align(chev, LV_ALIGN_TOP_LEFT, 56, 278);
}

/* ---- header ------------------------------------------------------------- */

static void build_header(lv_obj_t *p) {
    S.clock = lbl(p, F_CLOCK);
    lv_label_set_text(S.clock, "--:--");
    lv_obj_align(S.clock, LV_ALIGN_TOP_LEFT, MX - 2, 6);

    S.date = lbl(p, F_MED);
    lv_label_set_text(S.date, "");
    lv_obj_align(S.date, LV_ALIGN_TOP_LEFT, MX, 64);

    S.wx_icon = wx_icon(p, HOME_WX_SUN, 38);
    lv_obj_align(S.wx_icon, LV_ALIGN_TOP_RIGHT, -78, 12);

    S.wx_temp = lbl(p, F_BIG);
    lv_label_set_text(S.wx_temp, "--\xC2\xB0");
    lv_obj_align(S.wx_temp, LV_ALIGN_TOP_RIGHT, -14, 18);

    S.city = lbl(p, F_SML);
    lv_label_set_text(S.city, "");
    lv_obj_align(S.city, LV_ALIGN_TOP_RIGHT, -14, 56);
}

/* ---- forecast strip ----------------------------------------------------- */

#define FC_X0 (SB + 8)
#define FC_W  (W - FC_X0 - 8)

static void build_forecast(lv_obj_t *p) {
    for (int i = 0; i < HOME_FORECAST_MAX; i++) {
        int cx = FC_X0 + FC_W * (2 * i + 1) / (2 * HOME_FORECAST_MAX);

        S.fc[i].dow = lbl(p, F_SML);
        lv_label_set_text(S.fc[i].dow, "");
        lv_obj_set_style_text_align(S.fc[i].dow, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(S.fc[i].dow, LV_ALIGN_TOP_MID, cx - W / 2, 102);

        S.fc_wx[i] = HOME_WX_SUN;
        S.fc[i].icon = wx_icon(p, S.fc_wx[i], 30);
        lv_obj_align(S.fc[i].icon, LV_ALIGN_TOP_MID, cx - W / 2, 124);

        S.fc[i].temp = lbl(p, F_SML);
        lv_label_set_text(S.fc[i].temp, "");
        lv_obj_set_style_text_align(S.fc[i].temp, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(S.fc[i].temp, LV_ALIGN_TOP_MID, cx - W / 2, 158);
    }
}

/* ---- econ rows ---------------------------------------------------------- */

#define EC_X    MX          /* full-width: text starts at the main-area edge */
#define EC_Y0   193         /* first row's top y                          */
#define EC_DY   17          /* row pitch (3 rows fit the 188..246 band)   */
#define EC_VALW 58          /* right-aligned estimate/actual column width  */

/* A one-line label that ellipsizes on overflow: DOT mode only truncates (rather
 * than wrapping and growing) when the height is pinned to a single line. The
 * dense 12px font lets a long event name fit before it ever needs the dots. */
static lv_obj_t *eline(lv_obj_t *p, int w) {
    lv_obj_t *l = lbl(p, F_TINY);
    lv_obj_set_width(l, w);
    lv_obj_set_height(l, lv_font_get_line_height(F_TINY));
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_label_set_text(l, "");
    return l;
}

static void build_econ(lv_obj_t *p) {
    for (int i = 0; i < HOME_ECON_MAX; i++) {
        int y = EC_Y0 + i * EC_DY;

        S.ec[i].name = eline(p, RX - EC_X - EC_VALW - 6);
        lv_obj_align(S.ec[i].name, LV_ALIGN_TOP_LEFT, EC_X, y);

        S.ec[i].val = eline(p, EC_VALW);
        lv_obj_set_style_text_align(S.ec[i].val, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(S.ec[i].val, LV_ALIGN_TOP_RIGHT, -(W - RX), y);
    }
}

/* ---- status bar --------------------------------------------------------- */

#define ST_Y 262

static void build_status(lv_obj_t *p) {
    int seg = (W - SB) / 3;
    int c0 = SB + seg / 2, c1 = SB + seg + seg / 2, c2 = SB + 2 * seg + seg / 2;

    lv_obj_t *d = ui_icon(p, ICON_DROP, 22, 0);
    lv_obj_align(d, LV_ALIGN_TOP_LEFT, c0 - 34, ST_Y);
    S.humid = lbl(p, F_BIG);
    lv_label_set_text(S.humid, "--");
    lv_obj_align(S.humid, LV_ALIGN_TOP_LEFT, c0 - 6, ST_Y + 2);

    lv_obj_t *t = ui_icon(p, ICON_THERMO, 22, 0);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, c1 - 34, ST_Y);
    S.temp_in = lbl(p, F_BIG);
    lv_label_set_text(S.temp_in, "--");
    lv_obj_align(S.temp_in, LV_ALIGN_TOP_LEFT, c1 - 6, ST_Y + 2);

    /* nudged left so a 3-digit "100%" still clears the right panel edge */
    S.batt = lbl(p, F_BIG);
    lv_label_set_text(S.batt, "--");
    lv_obj_align(S.batt, LV_ALIGN_TOP_LEFT, c2 - 10, ST_Y + 2);
    S.batt_icon = ui_icon(p, ICON_BATTERY, 26, 80);
    lv_obj_align(S.batt_icon, LV_ALIGN_TOP_LEFT, c2 - 44, ST_Y + 1);

    vline(p, SB + seg,     ST_Y - 2, ST_Y + 26, 1, true);
    vline(p, SB + 2 * seg, ST_Y - 2, ST_Y + 26, 1, true);
}

/* ---- public API --------------------------------------------------------- */

void ui_home_create(lv_obj_t *page) {
    memset(&S, 0, sizeof(S));
    S.page = page;
    lv_obj_set_style_bg_opa(page, 255, 0);
    lv_obj_set_style_bg_color(page, WHITE, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    build_sidebar(page);
    build_header(page);
    build_forecast(page);
    build_econ(page);
    build_status(page);

    /* frame: sidebar divider + band dividers */
    vline(page, SB, 8, H - 8, 2, false);
    hline(page, SB + 6, W - 6, DIV_HEADER,   2, false);
    hline(page, SB + 6, W - 6, DIV_FORECAST, 2, false);
    hline(page, SB + 6, W - 6, DIV_ECON,     1, false);

    ui_home_tick();
}

static void set_ticker_row(int i, const char *sym, double price,
                           double percent, bool valid) {
    if (i < 0 || i >= HOME_TICKERS_MAX) return;
    lv_label_set_text(S.tk[i].sym, valid && sym[0] ? sym : "----");
    char buf[32];
    if (valid) {
        snprintf(buf, sizeof(buf), "$%.2f", price);
        lv_label_set_text(S.tk[i].price, buf);
        snprintf(buf, sizeof(buf), "%+.2f%%", percent);
        lv_label_set_text(S.tk[i].chg, buf);
        if (!S.tk[i].arrow)
            S.tk[i].arrow = ui_icon(S.page, percent >= 0 ? ICON_TRI_UP : ICON_TRI_DOWN, 12, 0);
        ui_icon_set(S.tk[i].arrow, percent >= 0 ? ICON_TRI_UP : ICON_TRI_DOWN, 0);
        lv_obj_clear_flag(S.tk[i].arrow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(S.tk[i].arrow, LV_ALIGN_TOP_LEFT, 14, TK_TOP[i] + 52);
    } else {
        lv_label_set_text(S.tk[i].price, "");
        lv_label_set_text(S.tk[i].chg, "");
        if (S.tk[i].arrow) lv_obj_add_flag(S.tk[i].arrow, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_home_set_quote(const stock_quote_t *q) {
    if (!q) return;
    set_ticker_row(0, q->symbol, q->price, q->percent, q->valid);
}

void ui_home_set_tickers(const home_ticker_t *t, int n) {
    if (!t) return;
    if (n > HOME_TICKERS_MAX) n = HOME_TICKERS_MAX;
    for (int i = 0; i < n; i++)
        set_ticker_row(i, t[i].symbol, t[i].price, t[i].percent, t[i].valid);
}

void ui_home_set_weather(home_wx_t wx, int temp_c, const char *city) {
    ui_icon_set(S.wx_icon, wx_to_icon(wx), 0);
    char buf[16];
    snprintf(buf, sizeof(buf), "%d\xC2\xB0" "C", temp_c);
    lv_label_set_text(S.wx_temp, buf);
    lv_obj_align(S.wx_temp, LV_ALIGN_TOP_RIGHT, -14, 18);
    if (city) lv_label_set_text(S.city, city);
    lv_obj_align(S.city, LV_ALIGN_TOP_RIGHT, -14, 56);
}

void ui_home_set_forecast(const home_forecast_t *days, int n) {
    if (!days) return;
    if (n > HOME_FORECAST_MAX) n = HOME_FORECAST_MAX;
    for (int i = 0; i < n; i++) {
        lv_label_set_text(S.fc[i].dow, days[i].dow);
        int cx = FC_X0 + FC_W * (2 * i + 1) / (2 * HOME_FORECAST_MAX);
        lv_obj_align(S.fc[i].dow, LV_ALIGN_TOP_MID, cx - W / 2, 102);

        if (days[i].wx != S.fc_wx[i]) {
            ui_icon_set(S.fc[i].icon, wx_to_icon(days[i].wx), 0);
            S.fc_wx[i] = days[i].wx;
        }

        char buf[16];
        snprintf(buf, sizeof(buf), "%d/%d", days[i].lo, days[i].hi);
        lv_label_set_text(S.fc[i].temp, buf);
        lv_obj_align(S.fc[i].temp, LV_ALIGN_TOP_MID, cx - W / 2, 158);

        lv_obj_clear_flag(S.fc[i].dow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(S.fc[i].icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(S.fc[i].temp, LV_OBJ_FLAG_HIDDEN);
    }
    /* blank any columns a short feed didn't fill (no phantom 'sunny' days) */
    for (int i = n; i < HOME_FORECAST_MAX; i++) {
        lv_obj_add_flag(S.fc[i].dow, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(S.fc[i].icon, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(S.fc[i].temp, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_home_set_env(const ui_env_t *env) {
    if (!env) return;
    char buf[24];
    if (env->env_valid) {
        snprintf(buf, sizeof(buf), "%.0f%%", env->humidity);
        lv_label_set_text(S.humid, buf);
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0" "C", env->temp_c);
        lv_label_set_text(S.temp_in, buf);
    }
    if (env->battery_valid) {
        snprintf(buf, sizeof(buf), "%d%%", env->battery_pct);
        lv_label_set_text(S.batt, buf);
        ui_icon_set(S.batt_icon, ICON_BATTERY, env->battery_pct);
    }
}

void ui_home_set_econ(const econ_event_t *evs, const char *const *when_labels, int n) {
    if (!S.ec[0].name) return;
    if (!evs || !when_labels) n = 0;
    if (n > HOME_ECON_MAX) n = HOME_ECON_MAX;
    if (n < 0) n = 0;

    for (int i = 0; i < n; i++) {
        const econ_event_t *ev = &evs[i];
        const char *when = when_labels[i] ? when_labels[i] : "";
        /* Compact the relative-day prefix so the event name keeps the most room:
         * today -> just the time, tomorrow -> "TMR", weekday/date kept as-is. */
        char w[20];
        if (strncmp(when, "TODAY ", 6) == 0)          snprintf(w, sizeof w, "%s", when + 6);
        else if (strncmp(when, "TOMORROW ", 9) == 0)  snprintf(w, sizeof w, "TMR %s", when + 9);
        else                                          snprintf(w, sizeof w, "%s", when);

        char buf[80];
        snprintf(buf, sizeof(buf), "[%s] %s", w, ev->event[0] ? ev->event : "--");
        lv_label_set_text(S.ec[i].name, buf);

        /* est -> act once the actual is released; otherwise just the estimate.
         * Plain ASCII so it renders in the stock Montserrat (no arrow glyph). */
        const char *est = ev->estimate[0] ? ev->estimate : "";
        const char *act = ev->actual[0] && strcmp(ev->actual, "--") != 0 ? ev->actual : "";
        if (act[0])
            snprintf(buf, sizeof(buf), "%s>%s", est[0] ? est : "?", act);
        else if (est[0])
            snprintf(buf, sizeof(buf), "%s", est);
        else
            buf[0] = '\0';
        lv_label_set_text(S.ec[i].val, buf);

        lv_obj_clear_flag(S.ec[i].name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(S.ec[i].val, LV_OBJ_FLAG_HIDDEN);
    }
    /* Hide rows a short feed didn't fill. */
    for (int i = n; i < HOME_ECON_MAX; i++) {
        lv_obj_add_flag(S.ec[i].name, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(S.ec[i].val, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_home_tick(void) {
    if (!S.clock) return;
    time_t now = time(NULL);
    struct tm lt;
    if (!localtime_r(&now, &lt)) return;
    char buf[40];
    strftime(buf, sizeof(buf), "%H:%M", &lt);
    lv_label_set_text(S.clock, buf);
    lv_obj_align(S.clock, LV_ALIGN_TOP_LEFT, MX - 2, 6);
    strftime(buf, sizeof(buf), "%a, %b %d", &lt);
    lv_label_set_text(S.date, buf);
    lv_obj_align(S.date, LV_ALIGN_TOP_LEFT, MX, 64);
}
