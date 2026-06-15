/*
 * ui_econ.c — LVGL economic-calendar view for the 400x300 monochrome panel.
 *
 * Layout (black on white only — the reflective panel binarizes at mid-gray):
 *   - title row:   "ECON  06-15 ~ 06-21"
 *   - up to ECON_EVENT_MAX one-line rows, each:
 *        left :  "MM-DD HH:MM CC *** Event name"   (truncated with "...")
 *        right:  "E<estimate> A<actual>"           (right-aligned)
 *   - footer:      navigation hint + "(+N more)" when the week overflowed
 * Loading / error / empty states reuse a single centered message label.
 */
#include "ui_econ.h"

#include <stdio.h>
#include <string.h>

#define SCR_W      400
#define SCR_H      300
#define ROW_TOP    30
#define ROW_H      20
#define RIGHT_W    96
#define FOOTER_Y   282

/* One source of truth for the navigation hint (the user's only control doc). */
#define FOOTER_NAV "KEY<prev  BOOT>next  K+B:home"

static const lv_color_t BLACK = LV_COLOR_MAKE(0, 0, 0);
static const lv_color_t WHITE = LV_COLOR_MAKE(0xff, 0xff, 0xff);

static struct {
    lv_obj_t *page;
    lv_obj_t *title;
    lv_obj_t *row_l[ECON_EVENT_MAX];
    lv_obj_t *row_r[ECON_EVENT_MAX];
    lv_obj_t *footer;
    lv_obj_t *msg;                 /* centered: loading / error / empty */
} S;

static lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, BLACK, 0);
    return l;
}

/* impact rank -> ASCII stars ("***"/"**"/"*"); the mono font has no U+2605. */
static const char *stars(int impact) {
    switch (impact) {
        case ECON_IMPACT_HIGH:   return "***";
        case ECON_IMPACT_MEDIUM: return "**";
        case ECON_IMPACT_LOW:    return "*";
        default:                 return "";
    }
}

static void show_rows(bool show) {
    for (int i = 0; i < ECON_EVENT_MAX; i++) {
        if (show) { lv_obj_clear_flag(S.row_l[i], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(S.row_r[i], LV_OBJ_FLAG_HIDDEN); }
        else      { lv_obj_add_flag(S.row_l[i], LV_OBJ_FLAG_HIDDEN);
                    lv_obj_add_flag(S.row_r[i], LV_OBJ_FLAG_HIDDEN); }
    }
}

void ui_econ_create(lv_obj_t *parent) {
    memset(&S, 0, sizeof(S));

    /* Full-screen opaque overlay so it fully covers the stock UI when shown. */
    S.page = lv_obj_create(parent);
    lv_obj_set_pos(S.page, 0, 0);
    lv_obj_set_size(S.page, SCR_W, SCR_H);
    lv_obj_set_style_bg_opa(S.page, 255, 0);
    lv_obj_set_style_bg_color(S.page, WHITE, 0);
    lv_obj_set_style_border_width(S.page, 0, 0);
    lv_obj_set_style_radius(S.page, 0, 0);
    lv_obj_set_style_pad_all(S.page, 0, 0);
    lv_obj_set_scrollbar_mode(S.page, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(S.page, LV_OBJ_FLAG_SCROLLABLE);

    S.title = mk_label(S.page, &lv_font_montserrat_16);
    lv_label_set_text(S.title, "ECON");
    lv_obj_align(S.title, LV_ALIGN_TOP_LEFT, 8, 4);

    /* divider under the title */
    static lv_point_precise_t line_pts[2];
    line_pts[0].x = 0;     line_pts[0].y = 26;
    line_pts[1].x = SCR_W; line_pts[1].y = 26;
    lv_obj_t *line = lv_line_create(S.page);
    lv_line_set_points(line, line_pts, 2);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_color(line, BLACK, 0);

    for (int i = 0; i < ECON_EVENT_MAX; i++) {
        int y = ROW_TOP + i * ROW_H;

        lv_obj_t *l = mk_label(S.page, &lv_font_montserrat_14);
        lv_obj_set_size(l, SCR_W - RIGHT_W - 12, ROW_H);
        lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);   /* truncate long names */
        lv_label_set_text(l, "");
        lv_obj_align(l, LV_ALIGN_TOP_LEFT, 6, y);
        S.row_l[i] = l;

        lv_obj_t *r = mk_label(S.page, &lv_font_montserrat_14);
        lv_obj_set_width(r, RIGHT_W);
        lv_obj_set_style_text_align(r, LV_TEXT_ALIGN_RIGHT, 0);
        lv_label_set_text(r, "");
        lv_obj_align(r, LV_ALIGN_TOP_RIGHT, -6, y);
        S.row_r[i] = r;
    }

    S.footer = mk_label(S.page, &lv_font_montserrat_14);
    lv_label_set_text(S.footer, "");
    lv_obj_align(S.footer, LV_ALIGN_TOP_LEFT, 6, FOOTER_Y);

    S.msg = mk_label(S.page, &lv_font_montserrat_20);
    lv_obj_set_width(S.msg, SCR_W - 32);
    lv_obj_set_style_text_align(S.msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(S.msg, LV_LABEL_LONG_WRAP);
    lv_label_set_text(S.msg, "");
    lv_obj_align(S.msg, LV_ALIGN_CENTER, 0, 10);
    lv_obj_add_flag(S.msg, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(S.page, LV_OBJ_FLAG_HIDDEN);   /* hidden until entered */
}

void ui_econ_show(bool show) {
    if (!S.page) return;
    if (show) { lv_obj_move_foreground(S.page); lv_obj_clear_flag(S.page, LV_OBJ_FLAG_HIDDEN); }
    else      { lv_obj_add_flag(S.page, LV_OBJ_FLAG_HIDDEN); }
}

static void set_title(const char *week_label) {
    char buf[ECON_LABEL_MAXLEN + 8];
    snprintf(buf, sizeof(buf), "ECON  %s", week_label ? week_label : "");
    lv_label_set_text(S.title, buf);
}

void ui_econ_set_loading(const char *week_label) {
    set_title(week_label);
    show_rows(false);
    lv_label_set_text(S.footer, "");
    lv_label_set_text(S.msg, "Loading...");
    lv_obj_clear_flag(S.msg, LV_OBJ_FLAG_HIDDEN);
}

void ui_econ_set_calendar(const econ_calendar_t *cal) {
    set_title(cal->week_label);

    if (!cal->valid) {                       /* error: show the message verbatim */
        show_rows(false);
        lv_label_set_text(S.footer, FOOTER_NAV);
        lv_label_set_text(S.msg, cal->error[0] ? cal->error : "error");
        lv_obj_clear_flag(S.msg, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (cal->count == 0) {                   /* valid but nothing this week */
        show_rows(false);
        lv_label_set_text(S.footer, FOOTER_NAV);
        lv_label_set_text(S.msg, "No high-impact events");
        lv_obj_clear_flag(S.msg, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(S.msg, LV_OBJ_FLAG_HIDDEN);
    show_rows(true);
    for (int i = 0; i < ECON_EVENT_MAX; i++) {
        if (i < cal->count) {
            const econ_event_t *e = &cal->items[i];
            char left[ECON_WHEN_MAXLEN + ECON_COUNTRY_MAXLEN + ECON_NAME_MAXLEN + 8];
            snprintf(left, sizeof(left), "%s %s %s %s",
                     e->when, e->country, stars(e->impact), e->event);
            lv_label_set_text(S.row_l[i], left);

            char right[ECON_FIELD_MAXLEN * 2 + 8];
            snprintf(right, sizeof(right), "E%s A%s", e->estimate, e->actual);
            lv_label_set_text(S.row_r[i], right);
        } else {
            lv_obj_add_flag(S.row_l[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(S.row_r[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    int more = cal->total_matched - cal->count;
    if (more > 0) {
        char footer[64];
        snprintf(footer, sizeof(footer), "KEY<prev  BOOT>next  (+%d more)", more);
        lv_label_set_text(S.footer, footer);
    } else {
        lv_label_set_text(S.footer, FOOTER_NAV);
    }
}
