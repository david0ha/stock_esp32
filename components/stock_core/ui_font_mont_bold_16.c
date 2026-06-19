/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --font /tmp/Montserrat-Bold.ttf --size 16 --bpp 1 --format lvgl -r 0x20-0x7E -r 0xB0 --no-compress -o components/stock_core/ui_font_mont_bold_16.c --lv-font-name ui_font_mont_bold_16
 ******************************************************************************/

#include "lvgl.h"

#ifndef UI_FONT_MONT_BOLD_16
#define UI_FONT_MONT_BOLD_16 1
#endif

#if UI_FONT_MONT_BOLD_16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xf8, 0x1f, 0xf0,

    /* U+0022 "\"" */
    0xde, 0xf7, 0xb0,

    /* U+0023 "#" */
    0x19, 0x83, 0x30, 0x66, 0x3f, 0xf7, 0xfe, 0x32,
    0x4, 0x47, 0xfe, 0xff, 0xc6, 0x60, 0xcc, 0x19,
    0x80,

    /* U+0024 "$" */
    0xc, 0x1f, 0x9f, 0xdd, 0xae, 0xc7, 0xe1, 0xf8,
    0x7e, 0xf, 0x87, 0xfb, 0xff, 0xe7, 0xe0, 0x60,
    0x30,

    /* U+0025 "%" */
    0x78, 0x67, 0xe6, 0x33, 0x21, 0x9b, 0xc, 0xd0,
    0x7f, 0xf9, 0xff, 0xe0, 0xb3, 0xd, 0x98, 0x4c,
    0xc6, 0x7e, 0x61, 0xe0,

    /* U+0026 "&" */
    0x3e, 0xf, 0xe1, 0xcc, 0x39, 0x83, 0xe0, 0x38,
    0x1f, 0xb7, 0x3e, 0xe3, 0xdc, 0x39, 0xff, 0x9f,
    0x20,

    /* U+0027 "'" */
    0xff,

    /* U+0028 "(" */
    0x3b, 0x9c, 0xee, 0x73, 0x9c, 0xe7, 0x38, 0xe7,
    0x38, 0xe0,

    /* U+0029 ")" */
    0xe3, 0x9c, 0xe3, 0x9c, 0xe7, 0x39, 0xce, 0xe7,
    0x3b, 0x80,

    /* U+002A "*" */
    0x10, 0xa9, 0xf3, 0xe5, 0x42, 0x0,

    /* U+002B "+" */
    0x18, 0x18, 0xff, 0xff, 0x18, 0x18, 0x18,

    /* U+002C "," */
    0xfd, 0xac,

    /* U+002D "-" */
    0xff, 0xc0,

    /* U+002E "." */
    0xff, 0x80,

    /* U+002F "/" */
    0x3, 0x7, 0x6, 0x6, 0xe, 0xc, 0xc, 0x1c,
    0x18, 0x18, 0x38, 0x30, 0x30, 0x70, 0x60, 0x60,

    /* U+0030 "0" */
    0x1e, 0x1f, 0xe7, 0x3b, 0x87, 0xe1, 0xf8, 0x7e,
    0x1f, 0x87, 0xe1, 0xdc, 0xe7, 0xf8, 0x78,

    /* U+0031 "1" */
    0xff, 0xce, 0x73, 0x9c, 0xe7, 0x39, 0xce, 0x70,

    /* U+0032 "2" */
    0x3e, 0x3f, 0x98, 0xe0, 0x70, 0x38, 0x3c, 0x3c,
    0x3c, 0x1c, 0x1c, 0x1f, 0xef, 0xf0,

    /* U+0033 "3" */
    0x7f, 0x9f, 0xe0, 0x30, 0x1c, 0xe, 0x3, 0xe0,
    0xfc, 0x7, 0x1, 0xd8, 0xf7, 0xf8, 0xfc,

    /* U+0034 "4" */
    0x7, 0x3, 0x81, 0xc0, 0x70, 0x38, 0x1c, 0xe6,
    0x3b, 0xff, 0xff, 0xc0, 0xe0, 0x38, 0xe,

    /* U+0035 "5" */
    0x7f, 0x3f, 0x98, 0xc, 0x6, 0x3, 0xf1, 0xfc,
    0x7, 0x3, 0xe1, 0xff, 0xcf, 0xc0,

    /* U+0036 "6" */
    0x1f, 0x1f, 0x9c, 0x5c, 0xe, 0x7, 0x73, 0xfd,
    0xe7, 0xe3, 0xb9, 0xdf, 0xc3, 0xc0,

    /* U+0037 "7" */
    0xff, 0xff, 0xf8, 0xfc, 0x60, 0x70, 0x38, 0x18,
    0x1c, 0xe, 0x6, 0x7, 0x3, 0x0,

    /* U+0038 "8" */
    0x3e, 0x1f, 0xce, 0x3b, 0x8e, 0xe3, 0x9f, 0xc7,
    0xfb, 0x87, 0xe1, 0xf8, 0x77, 0xf8, 0xfc,

    /* U+0039 "9" */
    0x3c, 0x3f, 0xb8, 0xdc, 0x7e, 0x3b, 0xfc, 0xf6,
    0x7, 0x3, 0x83, 0x9f, 0x8f, 0x80,

    /* U+003A ":" */
    0xff, 0x80, 0x3f, 0xe0,

    /* U+003B ";" */
    0xff, 0x80, 0x3f, 0x6b, 0x0,

    /* U+003C "<" */
    0x1, 0xf, 0x3e, 0xf0, 0xe0, 0xfc, 0x1f, 0x7,

    /* U+003D "=" */
    0xff, 0xff, 0x0, 0x0, 0xff, 0xff,

    /* U+003E ">" */
    0x80, 0xe0, 0x7c, 0x1f, 0x7, 0x3e, 0xf0, 0xc0,

    /* U+003F "?" */
    0x3e, 0x7f, 0xd8, 0xe0, 0x70, 0x78, 0x78, 0x38,
    0x38, 0x0, 0xe, 0x7, 0x3, 0x80,

    /* U+0040 "@" */
    0x7, 0xc0, 0x3f, 0xe0, 0xe0, 0xe3, 0x9d, 0xe6,
    0x7f, 0xd9, 0xce, 0xf3, 0xd, 0xe6, 0x1b, 0xcc,
    0x37, 0x9c, 0xed, 0x9f, 0xf3, 0x9c, 0xe3, 0x80,
    0x3, 0xfc, 0x1, 0xf8, 0x0,

    /* U+0041 "A" */
    0x7, 0x0, 0xf0, 0xf, 0x1, 0xf8, 0x1d, 0x81,
    0x9c, 0x39, 0xc3, 0xe, 0x7f, 0xe7, 0xfe, 0xe0,
    0x7e, 0x7,

    /* U+0042 "B" */
    0xff, 0x3f, 0xee, 0x1f, 0x87, 0xe1, 0xff, 0xcf,
    0xfb, 0x87, 0xe1, 0xf8, 0x7f, 0xff, 0xfc,

    /* U+0043 "C" */
    0xf, 0x87, 0xfd, 0xe3, 0x78, 0xe, 0x1, 0xc0,
    0x38, 0x7, 0x0, 0x70, 0xf, 0x18, 0xff, 0x87,
    0xc0,

    /* U+0044 "D" */
    0xff, 0x1f, 0xf3, 0x87, 0x70, 0xfe, 0xf, 0xc1,
    0xf8, 0x3f, 0x7, 0xe1, 0xfc, 0x3b, 0xfe, 0x7f,
    0x80,

    /* U+0045 "E" */
    0xff, 0xff, 0xf8, 0x1c, 0xe, 0x7, 0xfb, 0xfd,
    0xc0, 0xe0, 0x70, 0x3f, 0xff, 0xf0,

    /* U+0046 "F" */
    0xff, 0xff, 0xf8, 0x1c, 0xe, 0x7, 0x3, 0xfd,
    0xfe, 0xe0, 0x70, 0x38, 0x1c, 0x0,

    /* U+0047 "G" */
    0xf, 0x87, 0xfd, 0xe3, 0x78, 0xe, 0x1, 0xc0,
    0x38, 0x3f, 0x7, 0x70, 0xef, 0x1c, 0xff, 0x87,
    0xc0,

    /* U+0048 "H" */
    0xe1, 0xf8, 0x7e, 0x1f, 0x87, 0xe1, 0xff, 0xff,
    0xff, 0x87, 0xe1, 0xf8, 0x7e, 0x1f, 0x87,

    /* U+0049 "I" */
    0xff, 0xff, 0xff, 0xff, 0xf0,

    /* U+004A "J" */
    0x7f, 0x7f, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7,
    0x7, 0x47, 0xfe, 0x7c,

    /* U+004B "K" */
    0xe1, 0xdc, 0x3b, 0x8e, 0x73, 0x8e, 0xe1, 0xf8,
    0x3f, 0x87, 0xb8, 0xe7, 0x9c, 0x73, 0x87, 0x70,
    0x70,

    /* U+004C "L" */
    0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0, 0xe0,
    0xe0, 0xe0, 0xff, 0xff,

    /* U+004D "M" */
    0xe0, 0x3f, 0x1, 0xfc, 0x1f, 0xe0, 0xff, 0x8f,
    0xfe, 0x7f, 0xb6, 0xfd, 0xf7, 0xe7, 0x3f, 0x39,
    0xf8, 0x8f, 0xc0, 0x70,

    /* U+004E "N" */
    0xe1, 0xf8, 0x7f, 0x1f, 0xe7, 0xf9, 0xff, 0x7e,
    0xff, 0x9f, 0xe7, 0xf8, 0xfe, 0x1f, 0x87,

    /* U+004F "O" */
    0x1f, 0x3, 0xfc, 0x79, 0xef, 0xf, 0xe0, 0x7e,
    0x7, 0xe0, 0x7e, 0x7, 0x70, 0xf7, 0xe, 0x3f,
    0xc1, 0xf0,

    /* U+0050 "P" */
    0xff, 0x3f, 0xee, 0x3f, 0x87, 0xe1, 0xf8, 0x7e,
    0x3f, 0xfe, 0xff, 0x38, 0xe, 0x3, 0x80,

    /* U+0051 "Q" */
    0x1f, 0x81, 0xfe, 0x1c, 0x39, 0xe1, 0xee, 0x7,
    0x70, 0x3b, 0x81, 0xdc, 0xe, 0x70, 0xe3, 0xff,
    0xf, 0xe0, 0xe, 0x20, 0x3f, 0x80, 0xf0,

    /* U+0052 "R" */
    0xff, 0x3f, 0xee, 0x3f, 0x87, 0xe1, 0xf8, 0x7e,
    0x3f, 0xfe, 0xfe, 0x39, 0xce, 0x3b, 0x87,

    /* U+0053 "S" */
    0x3e, 0x3f, 0xb8, 0x5c, 0xf, 0x7, 0xf0, 0xfc,
    0xf, 0x3, 0xe1, 0xff, 0xcf, 0xc0,

    /* U+0054 "T" */
    0xff, 0xff, 0xf1, 0xc0, 0x70, 0x1c, 0x7, 0x1,
    0xc0, 0x70, 0x1c, 0x7, 0x1, 0xc0, 0x70,

    /* U+0055 "U" */
    0xe1, 0xf8, 0x7e, 0x1f, 0x87, 0xe1, 0xf8, 0x7e,
    0x1f, 0x87, 0xe1, 0xdc, 0xe7, 0xf8, 0x78,

    /* U+0056 "V" */
    0xe0, 0x7e, 0x7, 0x70, 0xe7, 0xe, 0x30, 0xc3,
    0x9c, 0x39, 0x81, 0xf8, 0x1f, 0x80, 0xf0, 0xf,
    0x0, 0xe0,

    /* U+0057 "W" */
    0x60, 0xe0, 0xdc, 0x38, 0x77, 0x1e, 0x19, 0xc7,
    0xc6, 0x31, 0xb3, 0x8e, 0xec, 0xe3, 0xbb, 0xb0,
    0x6c, 0xfc, 0x1f, 0x1f, 0x7, 0xc7, 0x80, 0xe1,
    0xe0, 0x38, 0x38,

    /* U+0058 "X" */
    0x70, 0xee, 0x38, 0xe6, 0xf, 0xc1, 0xf0, 0x1c,
    0x3, 0xc0, 0xf8, 0x3b, 0x87, 0x39, 0xc7, 0x70,
    0x70,

    /* U+0059 "Y" */
    0xe0, 0xec, 0x1d, 0xc7, 0x1c, 0xc3, 0xb8, 0x3e,
    0x7, 0xc0, 0x70, 0xe, 0x1, 0xc0, 0x38, 0x7,
    0x0,

    /* U+005A "Z" */
    0xff, 0xff, 0xf0, 0x38, 0x1e, 0x7, 0x3, 0x81,
    0xc0, 0xf0, 0x78, 0x1c, 0xf, 0xff, 0xff,

    /* U+005B "[" */
    0xff, 0xf9, 0xce, 0x73, 0x9c, 0xe7, 0x39, 0xce,
    0x7f, 0xe0,

    /* U+005C "\\" */
    0xe0, 0x60, 0x60, 0x70, 0x30, 0x30, 0x38, 0x18,
    0x18, 0x1c, 0xc, 0xc, 0xe, 0x6, 0x6, 0x7,

    /* U+005D "]" */
    0xff, 0xce, 0x73, 0x9c, 0xe7, 0x39, 0xce, 0x73,
    0xff, 0xe0,

    /* U+005E "^" */
    0x18, 0x38, 0x3c, 0x6c, 0x66, 0x46, 0xc2,

    /* U+005F "_" */
    0xff, 0xff,

    /* U+0060 "`" */
    0xe1, 0x86,

    /* U+0061 "a" */
    0x7c, 0x7e, 0x7, 0x7f, 0xff, 0xe7, 0xe7, 0xff,
    0x77,

    /* U+0062 "b" */
    0xe0, 0x38, 0xe, 0x3, 0xbc, 0xff, 0xbc, 0xfe,
    0x1f, 0x87, 0xe1, 0xfc, 0xff, 0xfb, 0xbc,

    /* U+0063 "c" */
    0x1e, 0x3f, 0xfc, 0x7c, 0xe, 0x7, 0x3, 0xc6,
    0xff, 0x1e, 0x0,

    /* U+0064 "d" */
    0x1, 0xc0, 0x70, 0x1c, 0xf7, 0x7f, 0xfc, 0xfe,
    0x1f, 0x87, 0xe1, 0xfc, 0xf7, 0xfc, 0xf7,

    /* U+0065 "e" */
    0x3e, 0x3f, 0xb8, 0xff, 0xff, 0xff, 0x3, 0x80,
    0xfe, 0x1e, 0x0,

    /* U+0066 "f" */
    0x3e, 0xf9, 0xc7, 0xef, 0xce, 0x1c, 0x38, 0x70,
    0xe1, 0xc3, 0x80,

    /* U+0067 "g" */
    0x3d, 0xdf, 0xff, 0x3f, 0x87, 0xe1, 0xfc, 0xf7,
    0xfc, 0xf7, 0x1, 0xd8, 0xf7, 0xf8, 0xfc,

    /* U+0068 "h" */
    0xe0, 0x70, 0x38, 0x1d, 0xcf, 0xf7, 0x9f, 0x8f,
    0xc7, 0xe3, 0xf1, 0xf8, 0xfc, 0x70,

    /* U+0069 "i" */
    0x5f, 0xaf, 0xff, 0xff, 0xfe,

    /* U+006A "j" */
    0x11, 0xce, 0x23, 0x9c, 0xe7, 0x39, 0xce, 0x73,
    0x9f, 0xfe,

    /* U+006B "k" */
    0xe0, 0x38, 0xe, 0x3, 0x8e, 0xe7, 0x3b, 0x8f,
    0xc3, 0xf8, 0xfe, 0x39, 0xce, 0x3b, 0x87,

    /* U+006C "l" */
    0xff, 0xff, 0xff, 0xff, 0xf0,

    /* U+006D "m" */
    0xfe, 0x79, 0xff, 0xfb, 0xcf, 0x3f, 0x1c, 0x7e,
    0x38, 0xfc, 0x71, 0xf8, 0xe3, 0xf1, 0xc7, 0xe3,
    0x8e,

    /* U+006E "n" */
    0xee, 0x7f, 0xbc, 0xfc, 0x7e, 0x3f, 0x1f, 0x8f,
    0xc7, 0xe3, 0x80,

    /* U+006F "o" */
    0x1e, 0x1f, 0xef, 0x3f, 0x87, 0xe1, 0xf8, 0x7f,
    0x3d, 0xfe, 0x1e, 0x0,

    /* U+0070 "p" */
    0xef, 0x3f, 0xef, 0x3f, 0x87, 0xe1, 0xf8, 0x7f,
    0x3f, 0xfe, 0xef, 0x38, 0xe, 0x3, 0x80,

    /* U+0071 "q" */
    0x3d, 0xdf, 0xff, 0x3f, 0x87, 0xe1, 0xf8, 0x7f,
    0x3d, 0xff, 0x3d, 0xc0, 0x70, 0x1c, 0x7,

    /* U+0072 "r" */
    0xff, 0xff, 0x38, 0xe3, 0x8e, 0x38, 0xe0,

    /* U+0073 "s" */
    0x3e, 0xfe, 0xe2, 0xf0, 0x7e, 0xf, 0x7, 0xff,
    0x7c,

    /* U+0074 "t" */
    0x70, 0xe3, 0xf7, 0xe7, 0xe, 0x1c, 0x38, 0x70,
    0xf8, 0xf8,

    /* U+0075 "u" */
    0xe3, 0xf1, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0x9e,
    0xff, 0x3b, 0x80,

    /* U+0076 "v" */
    0xe1, 0xf8, 0x66, 0x39, 0xcc, 0x37, 0xf, 0x83,
    0xe0, 0x78, 0x1c, 0x0,

    /* U+0077 "w" */
    0xe3, 0x86, 0xc7, 0x19, 0x8e, 0x33, 0xb6, 0x63,
    0x6d, 0x86, 0xdb, 0xf, 0x1e, 0xe, 0x38, 0x1c,
    0x70,

    /* U+0078 "x" */
    0x63, 0xb9, 0x8f, 0xc3, 0xc1, 0xc0, 0xf0, 0xfc,
    0xe6, 0xe3, 0x80,

    /* U+0079 "y" */
    0xe1, 0xf8, 0x66, 0x39, 0xcc, 0x37, 0xf, 0x81,
    0xe0, 0x78, 0x1c, 0x7, 0xf, 0x83, 0xc0,

    /* U+007A "z" */
    0xff, 0xff, 0xe, 0x1c, 0x3c, 0x38, 0x70, 0xff,
    0xff,

    /* U+007B "{" */
    0x3b, 0xdc, 0xe7, 0x3b, 0xde, 0x73, 0x9c, 0xe7,
    0x3c, 0xe0,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xf8,

    /* U+007D "}" */
    0xf3, 0xe3, 0x8e, 0x38, 0xe3, 0xcf, 0x38, 0xe3,
    0x8e, 0x3b, 0xef, 0x0,

    /* U+007E "~" */
    0x73, 0xff, 0xce,

    /* U+00B0 "°" */
    0x77, 0xe3, 0x1f, 0xb8
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 72, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 74, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 6, .adv_w = 112, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 9, .adv_w = 184, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 26, .adv_w = 163, .box_w = 9, .box_h = 15, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 43, .adv_w = 225, .box_w = 13, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 63, .adv_w = 186, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 59, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 81, .adv_w = 91, .box_w = 5, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 91, .adv_w = 92, .box_w = 5, .box_h = 15, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 101, .adv_w = 111, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 107, .adv_w = 153, .box_w = 8, .box_h = 7, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 114, .adv_w = 67, .box_w = 3, .box_h = 5, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 116, .adv_w = 99, .box_w = 5, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 118, .adv_w = 67, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 120, .adv_w = 100, .box_w = 8, .box_h = 16, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 136, .adv_w = 174, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 151, .adv_w = 100, .box_w = 5, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 151, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 173, .adv_w = 152, .box_w = 10, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 176, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 152, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 217, .adv_w = 163, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 159, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 245, .adv_w = 169, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 260, .adv_w = 163, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 274, .adv_w = 67, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 278, .adv_w = 67, .box_w = 3, .box_h = 11, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 283, .adv_w = 153, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 291, .adv_w = 153, .box_w = 8, .box_h = 6, .ofs_x = 1, .ofs_y = 3},
    {.bitmap_index = 297, .adv_w = 153, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 305, .adv_w = 151, .box_w = 9, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 319, .adv_w = 265, .box_w = 15, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 348, .adv_w = 196, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 366, .adv_w = 196, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 381, .adv_w = 185, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 398, .adv_w = 211, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 415, .adv_w = 172, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 429, .adv_w = 164, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 443, .adv_w = 197, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 460, .adv_w = 207, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 475, .adv_w = 84, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 480, .adv_w = 138, .box_w = 8, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 492, .adv_w = 189, .box_w = 11, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 509, .adv_w = 155, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 521, .adv_w = 244, .box_w = 13, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 541, .adv_w = 207, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 556, .adv_w = 216, .box_w = 12, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 574, .adv_w = 187, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 589, .adv_w = 216, .box_w = 13, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 612, .adv_w = 188, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 627, .adv_w = 163, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 641, .adv_w = 158, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 656, .adv_w = 202, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 671, .adv_w = 191, .box_w = 12, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 689, .adv_w = 298, .box_w = 18, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 716, .adv_w = 183, .box_w = 11, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 733, .adv_w = 173, .box_w = 11, .box_h = 12, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 750, .adv_w = 172, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 765, .adv_w = 94, .box_w = 5, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 775, .adv_w = 100, .box_w = 8, .box_h = 16, .ofs_x = -1, .ofs_y = -2},
    {.bitmap_index = 791, .adv_w = 94, .box_w = 5, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 801, .adv_w = 154, .box_w = 8, .box_h = 7, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 808, .adv_w = 128, .box_w = 8, .box_h = 2, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 810, .adv_w = 154, .box_w = 5, .box_h = 3, .ofs_x = 1, .ofs_y = 10},
    {.bitmap_index = 812, .adv_w = 158, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 821, .adv_w = 177, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 836, .adv_w = 151, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 847, .adv_w = 177, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 862, .adv_w = 162, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 873, .adv_w = 99, .box_w = 7, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 884, .adv_w = 179, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 899, .adv_w = 177, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 913, .adv_w = 77, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 918, .adv_w = 79, .box_w = 5, .box_h = 16, .ofs_x = -1, .ofs_y = -3},
    {.bitmap_index = 928, .adv_w = 168, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 943, .adv_w = 77, .box_w = 3, .box_h = 12, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 948, .adv_w = 269, .box_w = 15, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 965, .adv_w = 177, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 976, .adv_w = 168, .box_w = 10, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 988, .adv_w = 177, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1003, .adv_w = 177, .box_w = 10, .box_h = 12, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1018, .adv_w = 110, .box_w = 6, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1025, .adv_w = 136, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1034, .adv_w = 111, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1044, .adv_w = 176, .box_w = 9, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1055, .adv_w = 153, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1067, .adv_w = 240, .box_w = 15, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1084, .adv_w = 152, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1095, .adv_w = 153, .box_w = 10, .box_h = 12, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1110, .adv_w = 139, .box_w = 8, .box_h = 9, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 1119, .adv_w = 100, .box_w = 5, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1129, .adv_w = 79, .box_w = 3, .box_h = 15, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 1135, .adv_w = 100, .box_w = 6, .box_h = 15, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 1147, .adv_w = 153, .box_w = 8, .box_h = 3, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 1150, .adv_w = 107, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 6}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 176, .range_length = 1, .glyph_id_start = 96,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Map glyph_ids to kern left classes*/
static const uint8_t kern_left_class_mapping[] =
{
    0, 0, 1, 2, 0, 3, 4, 5,
    2, 6, 0, 7, 8, 9, 8, 9,
    10, 11, 0, 12, 13, 14, 15, 16,
    17, 18, 11, 19, 19, 0, 0, 0,
    20, 21, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 22, 23, 0, 0,
    24, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 22, 0, 0, 8,
    25
};

/*Map glyph_ids to kern right classes*/
static const uint8_t kern_right_class_mapping[] =
{
    0, 0, 1, 2, 0, 3, 4, 5,
    2, 0, 6, 7, 8, 9, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 11,
    17, 18, 19, 20, 20, 0, 0, 0,
    21, 22, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 23, 24, 25, 0,
    26, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 25, 8,
    27
};

/*Kern values between classes*/
static const int8_t kern_class_values[] =
{
    0, 1, 0, 0, 0, 0, 2, 0,
    1, 0, 0, 3, 0, 0, 0, 0,
    1, 0, 0, 0, 0, 0, 0, 0,
    1, 0, 0, 1, 12, 0, 6, -4,
    0, 5, 0, -14, -15, 1, 11, 4,
    4, -10, 1, 10, 0, 9, 3, 7,
    -4, 0, 15, 3, -1, 5, 0, -8,
    0, 0, 0, 0, -5, 4, 5, 0,
    0, -3, 0, -1, 3, 0, -3, 0,
    -3, -1, -5, 0, 0, -3, 0, -5,
    -4, 0, -6, 0, -32, 0, -5, -13,
    5, 8, 0, 0, -5, 3, 3, 8,
    5, -4, 5, 0, 0, -14, 0, 0,
    -9, 0, 0, -6, -3, -10, 0, -10,
    -1, 0, -8, 0, -1, 8, 0, -8,
    -3, -1, 1, 0, -4, 0, 0, -2,
    -17, 0, 0, -20, -3, 5, -13, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    9, 0, 3, 0, 0, -5, 0, 0,
    0, 0, 0, 0, 0, 0, 11, 3,
    1, 0, 2, 5, 3, 8, -3, 0,
    5, -3, -9, -34, 1, 6, 5, -1,
    -3, 0, 6, 0, 7, 0, 7, 0,
    0, 0, 1, -3, 8, 0, 0, -4,
    -9, 0, 0, -3, 1, -2, 0, 1,
    -5, -4, -5, 1, 0, -3, 0, 0,
    0, -10, 1, 0, -17, 0, 0, 0,
    1, -14, 3, -17, 0, 0, -9, -2,
    0, 23, -3, -4, 3, 3, 0, 0,
    -4, 3, 0, 0, -14, -5, 0, -24,
    0, 3, -17, 0, 15, -5, 0, -9,
    9, 0, -18, -24, -19, -5, 8, 0,
    0, -16, 0, 2, -7, 0, -4, 0,
    -5, 0, 4, 8, -30, 11, 0, 1,
    0, 0, 0, 0, 1, 1, -3, -5,
    0, -1, -1, -3, 0, 0, -1, 0,
    0, 0, -5, 0, 0, -5, 0, -5,
    0, 0, 0, 0, 3, -1, 0, 0,
    -1, 3, 3, 0, 0, 0, 0, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 8, 0, 0, -3, 0, -3,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, -1, 0, -3, -2, 0, 0, 0,
    0, 0, 0, 0, 0, 0, -4, 0,
    -8, -1, -8, 5, 0, -5, 3, 5,
    6, 0, -6, 0, -2, 0, 0, -11,
    3, -1, 2, -14, 3, 0, -13, 0,
    5, -8, 0, 0, 0, -3, 0, 0,
    -3, 0, 0, 0, 0, 0, -1, -1,
    0, -1, -3, 0, 0, 0, 0, 0,
    0, -3, 0, 0, -2, 0, -1, 0,
    -5, 3, 0, -2, 1, 3, 3, 0,
    0, 0, 0, 0, 0, -1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -4,
    0, 8, -1, 1, -7, 0, 6, -13,
    -13, -10, -5, 3, 0, -2, -17, -4,
    0, -4, 0, -5, 4, -4, 0, 3,
    2, -8, 3, 0, 0, 0, -3, 0,
    0, 1, 0, 3, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, -3,
    0, 0, -8, 0, 0, 0, 0, 3,
    0, 0, 0, 0, 0, 0, 0, 14,
    0, 0, 0, 0, 0, 0, 1, 0,
    0, 0, -3, 0, 0, -5, 0, 3,
    0, -1, 0, 0, 0, -3, 0, 2,
    0, -13, -8, 0, 0, 0, -4, -13,
    0, 0, -3, 3, 0, -7, -1, -2,
    0, 0, -4, 3, 0, -4, 0, 0,
    0, 2, 0, 1, -5, -5, 0, -3,
    -3, -3, 0, 0, 0, 0, 0, 0,
    -8, 0, 0, -5, 3, -8, 3, 1,
    3, 0, 0, 0, 3, 1, -1, 0,
    11, 0, 6, 1, 1, -4, 0, 5,
    0, 0, 0, 3, 0, 0, 8, 0,
    8, 0, 0, -15, 0, -3, 4, 8,
    -34, 0, 23, 2, -5, -5, 3, 3,
    -1, 1, -13, 0, 0, 14, -15, -5,
    0, -19, 11, 36, -15, 0, -1, 5,
    -6, 0, 1, -3, 0, 3, 31, -5,
    -2, 7, 6, -6, 3, 0, 0, 3,
    3, -5, -8, 0, -33, 8, 0, 0,
    0, 5, 5, 7, 0, 0, 8, 0,
    -17, -14, 0, 12, 8, 5, -10, 1,
    9, 0, 9, 0, 5, 3, 0, 14,
    0, 0, 0
};


/*Collect the kern class' data in one place*/
static const lv_font_fmt_txt_kern_classes_t kern_classes =
{
    .class_pair_values   = kern_class_values,
    .left_class_mapping  = kern_left_class_mapping,
    .right_class_mapping = kern_right_class_mapping,
    .left_class_cnt      = 25,
    .right_class_cnt     = 27,
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_classes,
    .kern_scale = 16,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 1,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t ui_font_mont_bold_16 = {
#else
lv_font_t ui_font_mont_bold_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 17,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if UI_FONT_MONT_BOLD_16*/

