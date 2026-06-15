/*
 * ui_fonts.h — custom heavy display fonts for the bold home card.
 *
 * Generated from Arial Black (a true black weight, matching the reference photo)
 * with lv_font_conv at 1-bpp for crisp rendering on the 1-bit reflective panel.
 * Regenerate, if ever needed, with:
 *   npx -y lv_font_conv --font "Arial Black.ttf" --size N --bpp 1 --format lvgl \
 *       -r 0x20-0x7E -r 0xB0 --no-compress -o ui_font_black_N.c --lv-font-name ui_font_black_N
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t ui_font_black_50;   /* symbol + price hero */
extern const lv_font_t ui_font_black_26;   /* time/date, change, weather values */

#ifdef __cplusplus
}
#endif
