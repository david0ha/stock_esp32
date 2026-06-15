/*
 * ui_home.c — bold ticker-card home screen for the 400x300 reflective mono panel.
 *
 * Modelled on the reference photo (sim/refs/ref_pltr.png): heavy black-on-white
 * type, a time/date line up top, the SYMBOL on the left, a DOMINANT PRICE on the
 * right with the day's change directly underneath — plus a weather strip
 * (temperature, humidity, battery) at the bottom since this is also the device's
 * clock screen.
 *
 * Weight comes from real heavy fonts (Arial Black, 1-bpp) declared in ui_fonts.h,
 * so glyphs are crisp and black on the binarizing panel without any faux-bold.
 */
#include "ui_home.h"
#include "ui_fonts.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define W 400
#define H 300

#define F_HERO (&ui_font_black_50)   /* symbol + price */
#define F_TEXT (&ui_font_black_26)   /* time/date, change, weather values */
#define F_CAP  (&lv_font_montserrat_14)

static const lv_color_t BLACK = LV_COLOR_MAKE(0, 0, 0);
static const lv_color_t WHITE = LV_COLOR_MAKE(0xff, 0xff, 0xff);

static struct {
    lv_obj_t *time;
    lv_obj_t *date;
    lv_obj_t *symbol;
    lv_obj_t *price;
    lv_obj_t *change;
    lv_obj_t *temp;
    lv_obj_t *humid;
    lv_obj_t *batt;
    stock_quote_t q;
    ui_env_t      env;
} S;

static lv_obj_t *mk(lv_obj_t *p, const lv_font_t *f) {
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, BLACK, 0);
    return l;
}

static void rule(lv_obj_t *page, int y, int inset, int width) {
    static lv_point_precise_t pts[4][2];
    static int n = 0;
    int i = n++ % 4;
    pts[i][0].x = inset;     pts[i][0].y = y;
    pts[i][1].x = W - inset; pts[i][1].y = y;
    lv_obj_t *l = lv_line_create(page);
    lv_line_set_points(l, pts[i], 2);
    lv_obj_set_style_line_width(l, width, 0);
    lv_obj_set_style_line_color(l, BLACK, 0);
}

/* layout anchors */
#define HERO_Y    62
#define CHANGE_Y  120

void ui_home_create(lv_obj_t *page) {
    memset(&S, 0, sizeof(S));
    lv_obj_set_style_bg_opa(page, 255, 0);
    lv_obj_set_style_bg_color(page, WHITE, 0);
    lv_obj_set_style_pad_all(page, 0, 0);
    lv_obj_set_style_border_width(page, 0, 0);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);

    /* top: time (left) + date (right) */
    S.time = mk(page, F_TEXT);
    lv_label_set_text(S.time, "--:--");
    lv_obj_align(S.time, LV_ALIGN_TOP_LEFT, 16, 12);

    S.date = mk(page, F_TEXT);
    lv_label_set_text(S.date, "");
    lv_obj_align(S.date, LV_ALIGN_TOP_RIGHT, -16, 14);

    rule(page, 52, 12, 2);

    /* hero: SYMBOL left + DOMINANT price right (tops aligned -> baselines aligned) */
    S.symbol = mk(page, F_HERO);
    lv_label_set_text(S.symbol, "----");
    lv_obj_align(S.symbol, LV_ALIGN_TOP_LEFT, 16, HERO_Y);

    S.price = mk(page, F_HERO);
    lv_label_set_text(S.price, "--");
    lv_obj_align(S.price, LV_ALIGN_TOP_RIGHT, -16, HERO_Y);

    S.change = mk(page, F_TEXT);
    lv_label_set_text(S.change, "");
    lv_obj_align(S.change, LV_ALIGN_TOP_RIGHT, -16, CHANGE_Y);

    rule(page, 170, 12, 2);

    /* bottom strip: temp | humidity | battery */
    S.temp = mk(page, F_TEXT);
    lv_label_set_text(S.temp, "--");
    lv_obj_align(S.temp, LV_ALIGN_BOTTOM_LEFT, 20, -38);
    lv_obj_t *tl = mk(page, F_CAP);
    lv_label_set_text(tl, "TEMP");
    lv_obj_align(tl, LV_ALIGN_BOTTOM_LEFT, 22, -16);

    S.humid = mk(page, F_TEXT);
    lv_label_set_text(S.humid, "--");
    lv_obj_align(S.humid, LV_ALIGN_BOTTOM_MID, 0, -38);
    lv_obj_t *hl = mk(page, F_CAP);
    lv_label_set_text(hl, "HUMIDITY");
    lv_obj_align(hl, LV_ALIGN_BOTTOM_MID, 0, -16);

    S.batt = mk(page, F_TEXT);
    lv_label_set_text(S.batt, "--");
    lv_obj_align(S.batt, LV_ALIGN_BOTTOM_RIGHT, -24, -38);
    lv_obj_t *bl = mk(page, F_CAP);
    lv_label_set_text(bl, "BATTERY");
    lv_obj_align(bl, LV_ALIGN_BOTTOM_RIGHT, -24, -16);

    ui_home_tick();
}

void ui_home_set_quote(const stock_quote_t *q) {
    if (!q) return;
    S.q = *q;

    const char *sym = (q->valid && q->symbol[0]) ? q->symbol : "----";
    lv_label_set_text(S.symbol, sym);
    lv_obj_align(S.symbol, LV_ALIGN_TOP_LEFT, 16, HERO_Y);

    char buf[48];
    if (q->valid) {
        snprintf(buf, sizeof(buf), "$%.2f", q->price);
        lv_label_set_text(S.price, buf);
        char sign = q->change >= 0 ? '+' : '-';
        double pct = q->percent < 0 ? -q->percent : q->percent;
        snprintf(buf, sizeof(buf), "1 day %c %.2f%%", sign, pct);
        lv_label_set_text(S.change, buf);
    } else {
        lv_label_set_text(S.price, "--");
        lv_label_set_text(S.change, "");
    }
    lv_obj_align(S.price, LV_ALIGN_TOP_RIGHT, -16, HERO_Y);
    lv_obj_align(S.change, LV_ALIGN_TOP_RIGHT, -16, CHANGE_Y);
}

void ui_home_set_env(const ui_env_t *env) {
    if (!env) return;
    S.env = *env;

    char buf[24];
    if (env->env_valid) {
        snprintf(buf, sizeof(buf), "%.1f\xC2\xB0" "C", env->temp_c);
        lv_label_set_text(S.temp, buf);
        lv_obj_align(S.temp, LV_ALIGN_BOTTOM_LEFT, 20, -38);
        snprintf(buf, sizeof(buf), "%.0f%%", env->humidity);
        lv_label_set_text(S.humid, buf);
        lv_obj_align(S.humid, LV_ALIGN_BOTTOM_MID, 0, -38);
    } else {
        lv_label_set_text(S.temp, "--");
        lv_label_set_text(S.humid, "--");
    }

    if (env->battery_valid) {
        snprintf(buf, sizeof(buf), "%d%%", env->battery_pct);
        lv_label_set_text(S.batt, buf);
    } else {
        lv_label_set_text(S.batt, "--");
    }
    lv_obj_align(S.batt, LV_ALIGN_BOTTOM_RIGHT, -24, -38);
}

void ui_home_tick(void) {
    if (!S.time) return;
    time_t now = time(NULL);
    struct tm lt;
    if (!localtime_r(&now, &lt)) return;
    char buf[40];
    strftime(buf, sizeof(buf), "%H:%M", &lt);
    lv_label_set_text(S.time, buf);
    lv_obj_align(S.time, LV_ALIGN_TOP_LEFT, 16, 12);
    strftime(buf, sizeof(buf), "%a %d %b %Y", &lt);
    lv_label_set_text(S.date, buf);
    lv_obj_align(S.date, LV_ALIGN_TOP_RIGHT, -16, 14);
}
