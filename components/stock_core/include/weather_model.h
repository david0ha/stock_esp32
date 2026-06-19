/*
 * weather_model.h — portable data model for the Open-Meteo weather feed.
 *
 * Mirrors the stock_model / econ_model split: a board-agnostic struct filled by
 * the parser/service (host-tested) and rendered by ui_home. Open-Meteo is free
 * and keyless, so geocoding (a typed city -> lat/lon) and the current + 7-day
 * forecast both come from the same provider with no secret to manage.
 *
 * The reflective mono panel can only express four weather glyphs, so the WMO
 * weather codes are collapsed to wx_kind_t. Its values intentionally match
 * home_wx_t (ui_home.h) one-for-one so the UI bridge is a plain cast.
 */
#pragma once

#include <stdbool.h>

#define WX_NAME_MAXLEN    40   /* resolved place name, e.g. "Seoul"            */
#define WX_COUNTRY_MAXLEN 4    /* ISO-3166 alpha-2 country code, e.g. "KR"     */
#define WX_FORECAST_MAX   7    /* days the home strip shows                    */

/* The four states the panel can draw clearly (see ui_icons.c). Snow/fog/storm
 * all collapse onto the nearest of these. Order matches home_wx_t. */
typedef enum {
    WX_SUN    = 0,
    WX_PARTLY = 1,
    WX_CLOUD  = 2,
    WX_RAIN   = 3,
} wx_kind_t;

/* A geocoded location: the top match for a typed query. */
typedef struct {
    char   name[WX_NAME_MAXLEN];       /* "Seoul"                 */
    char   country[WX_COUNTRY_MAXLEN]; /* "KR" (may be empty)     */
    double lat;
    double lon;
    bool   valid;
} geo_loc_t;

/* One forecast column. */
typedef struct {
    char      dow[4];   /* "FRI" (derived from the date)  */
    wx_kind_t wx;
    int       lo;       /* rounded °C low  */
    int       hi;       /* rounded °C high */
} wx_day_t;

/* Current conditions + multi-day forecast for one location. */
typedef struct {
    bool      now_valid;            /* current block parsed                 */
    wx_kind_t now_wx;
    int       now_temp_c;           /* rounded °C                           */
    int       day_count;            /* valid entries in days[] (<= MAX)     */
    wx_day_t  days[WX_FORECAST_MAX];
    bool      valid;                /* whole fetch+parse succeeded          */
} weather_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Collapse a WMO weather-interpretation code (0..99) to one of the four panel
 * glyphs. Unknown codes fall back to WX_CLOUD. */
wx_kind_t wx_from_wmo(int code);

#ifdef __cplusplus
}
#endif
