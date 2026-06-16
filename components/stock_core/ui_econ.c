/*
 * ui_econ.c — LVGL economic-calendar view for the 400x300 monochrome panel,
 * styled after investing.com's mobile calendar: one event per two-line "card"
 * inside a ruled grid.
 *
 *   ┌ ECON ─────────────────────── 06-15 ~ 06-21 ┐   title + thick rule
 *   │ Wed 12:00  JPY  ***  BoJ Interest Rate Dec… │   line 1: when/cur/impact/event
 *   │ Act 1.00%   │  Fcst 1.00%   │  Prev 0.75%   │   line 2: 3 cells, ruled
 *   ├─────────────────────────────────────────────┤   row separator
 *   ...
 *
 * Black on white only (the reflective panel binarizes at mid-gray). Loading /
 * error / empty states reuse a single centered message label.
 */
#include "ui_econ.h"

#include <stdio.h>
#include <string.h>

#define SCR_W      400
#define SCR_H      300

#define TITLE_Y    3
#define DIV_Y      22          /* thick rule under the title          */
#define BODY_TOP   25
#define EVT_H      35          /* per-event card height (2 text lines) */
#define L1_DY      2           /* line-1 y offset inside a card        */
#define L2_DY      18          /* line-2 y offset inside a card        */
#define FOOTER_Y   276

/* line-2 value columns: Act | Fcst | Prev */
#define COL1_X     6
#define VSEP1_X    134
#define COL2_X     140
#define VSEP2_X    266
#define COL3_X     272
#define CELL_W     124

#define EVT_N      ECON_EVENT_MAX

static const lv_color_t BLACK = LV_COLOR_MAKE(0, 0, 0);
static const lv_color_t WHITE = LV_COLOR_MAKE(0xff, 0xff, 0xff);

static struct {
    lv_obj_t *page;
    lv_obj_t *title;
    lv_obj_t *footer;
    lv_obj_t *msg;                 /* centered: loading / error / empty */
    struct {
        lv_obj_t *head;            /* line 1: when / currency / stars / event */
        lv_obj_t *val[3];          /* line 2: Act / Fcst / Prev               */
        lv_obj_t *vsep[2];         /* line-2 vertical rules                    */
        lv_obj_t *hsep;            /* card bottom rule                        */
    } ev[EVT_N];
} S;

/* Persistent point storage (LVGL keeps the pointer we hand to set_points). */
static lv_point_precise_t s_title_pts[2];
static lv_point_precise_t s_hsep_pts[EVT_N][2];
static lv_point_precise_t s_vsep_pts[EVT_N][2][2];

static lv_obj_t *mk_label(lv_obj_t *parent, const lv_font_t *font) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, BLACK, 0);
    return l;
}

static lv_obj_t *mk_line(lv_obj_t *parent, lv_point_precise_t *pts,
                         int x0, int y0, int x1, int y1, int w) {
    pts[0].x = x0; pts[0].y = y0;
    pts[1].x = x1; pts[1].y = y1;
    lv_obj_t *ln = lv_line_create(parent);
    lv_line_set_points(ln, pts, 2);
    lv_obj_set_style_line_width(ln, w, 0);
    lv_obj_set_style_line_color(ln, BLACK, 0);
    return ln;
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

static void show_event(int i, bool show) {
    lv_obj_t *objs[] = { S.ev[i].head, S.ev[i].val[0], S.ev[i].val[1],
                         S.ev[i].val[2], S.ev[i].vsep[0], S.ev[i].vsep[1],
                         S.ev[i].hsep };
    for (size_t k = 0; k < sizeof(objs) / sizeof(objs[0]); k++) {
        if (show) lv_obj_clear_flag(objs[k], LV_OBJ_FLAG_HIDDEN);
        else      lv_obj_add_flag(objs[k], LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_all_events(bool show) {
    for (int i = 0; i < EVT_N; i++) show_event(i, show);
}

void ui_econ_create(lv_obj_t *parent) {
    memset(&S, 0, sizeof(S));

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
    lv_obj_align(S.title, LV_ALIGN_TOP_LEFT, 8, TITLE_Y);
    mk_line(S.page, s_title_pts, 0, DIV_Y, SCR_W, DIV_Y, 3);   /* thick rule */

    for (int i = 0; i < EVT_N; i++) {
        int yb = BODY_TOP + i * EVT_H;

        lv_obj_t *h = mk_label(S.page, &lv_font_montserrat_14);
        lv_obj_set_size(h, SCR_W - 12, 16);
        lv_label_set_long_mode(h, LV_LABEL_LONG_DOT);
        lv_label_set_text(h, "");
        lv_obj_align(h, LV_ALIGN_TOP_LEFT, 6, yb + L1_DY);
        S.ev[i].head = h;

        const int colx[3] = { COL1_X, COL2_X, COL3_X };
        for (int c = 0; c < 3; c++) {
            lv_obj_t *v = mk_label(S.page, &lv_font_montserrat_14);
            lv_obj_set_size(v, c == 2 ? SCR_W - COL3_X - 4 : CELL_W, 16);
            lv_label_set_long_mode(v, LV_LABEL_LONG_CLIP);
            lv_label_set_text(v, "");
            lv_obj_align(v, LV_ALIGN_TOP_LEFT, colx[c], yb + L2_DY);
            S.ev[i].val[c] = v;
        }

        /* line-2 vertical rules + card bottom rule */
        int vy0 = yb + L2_DY - 1, vy1 = yb + EVT_H - 2;
        S.ev[i].vsep[0] = mk_line(S.page, s_vsep_pts[i][0], VSEP1_X, vy0, VSEP1_X, vy1, 1);
        S.ev[i].vsep[1] = mk_line(S.page, s_vsep_pts[i][1], VSEP2_X, vy0, VSEP2_X, vy1, 1);
        S.ev[i].hsep    = mk_line(S.page, s_hsep_pts[i], 0, yb + EVT_H, SCR_W, yb + EVT_H, 1);
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

    lv_obj_add_flag(S.page, LV_OBJ_FLAG_HIDDEN);
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

/* show only the centered message (loading / error / empty) */
static void show_message(const char *week_label, const char *footer, const char *text) {
    set_title(week_label);
    show_all_events(false);
    lv_label_set_text(S.footer, footer);
    lv_label_set_text(S.msg, text);
    lv_obj_clear_flag(S.msg, LV_OBJ_FLAG_HIDDEN);
}

void ui_econ_set_loading(const char *week_label) {
    show_message(week_label, "", "Loading...");
}

void ui_econ_set_calendar(const econ_calendar_t *cal) {
    if (!cal->valid) {
        show_message(cal->week_label, "KEY<prev  BOOT>next  K+B:home",
                     cal->error[0] ? cal->error : "error");
        return;
    }
    if (cal->count == 0) {
        show_message(cal->week_label, "KEY<prev  BOOT>next  K+B:home",
                     "No events this week");
        return;
    }

    set_title(cal->week_label);
    lv_obj_add_flag(S.msg, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < EVT_N; i++) {
        if (i >= cal->count) { show_event(i, false); continue; }
        show_event(i, true);
        const econ_event_t *e = &cal->items[i];

        char head[ECON_WHEN_MAXLEN + ECON_COUNTRY_MAXLEN + ECON_NAME_MAXLEN + 12];
        snprintf(head, sizeof(head), "%s  %s  %s  %s",
                 e->when, e->country, stars(e->impact), e->event);
        lv_label_set_text(S.ev[i].head, head);

        char cell[ECON_FIELD_MAXLEN + 8];
        snprintf(cell, sizeof(cell), "Act %s",  e->actual);   lv_label_set_text(S.ev[i].val[0], cell);
        snprintf(cell, sizeof(cell), "Fcst %s", e->estimate); lv_label_set_text(S.ev[i].val[1], cell);
        snprintf(cell, sizeof(cell), "Prev %s", e->previous); lv_label_set_text(S.ev[i].val[2], cell);
    }

    int more = cal->total_matched - cal->count;
    if (more > 0) {
        char footer[64];
        snprintf(footer, sizeof(footer), "KEY<prev  BOOT>next  (+%d more)", more);
        lv_label_set_text(S.footer, footer);
    } else {
        lv_label_set_text(S.footer, "KEY<prev  BOOT>next  K+B:home");
    }
}
