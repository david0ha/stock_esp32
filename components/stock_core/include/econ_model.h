/*
 * econ_model.h — data model for the FMP economic-calendar view.
 *
 * Mirrors the stock_model split: a portable, board-agnostic struct filled by the
 * parser/service (host-tested) and rendered by ui_econ. One screen = one week.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Two-line (investing.com-style) cards: the 400x300 panel fits 7 per screen. */
#define ECON_EVENT_MAX      7
#define ECON_COUNTRY_MAXLEN 6    /* ISO country/area code: "US", "JP", "EU"     */
#define ECON_NAME_MAXLEN    40   /* event name (UI truncates with "..." anyway) */
#define ECON_FIELD_MAXLEN   12   /* estimate / actual / previous as display text */
#define ECON_WHEN_MAXLEN    12   /* "MM-DD HH:MM"                                */
#define ECON_LABEL_MAXLEN   24   /* "06-15 ~ 06-21"                             */
#define ECON_ERR_MAXLEN     96   /* on-screen error message                     */

/* Importance ranks, matching FMP's "Low"/"Medium"/"High". 0 = unknown/none. */
typedef enum {
    ECON_IMPACT_NONE   = 0,
    ECON_IMPACT_LOW    = 1,
    ECON_IMPACT_MEDIUM = 2,
    ECON_IMPACT_HIGH   = 3,
} econ_impact_t;

typedef struct {
    char    when[ECON_WHEN_MAXLEN];        /* device-local "MM-DD HH:MM"        */
    char    country[ECON_COUNTRY_MAXLEN];
    char    event[ECON_NAME_MAXLEN];
    char    estimate[ECON_FIELD_MAXLEN];   /* "--" when the field is absent     */
    char    actual[ECON_FIELD_MAXLEN];     /* "--" when not yet released        */
    char    previous[ECON_FIELD_MAXLEN];
    int     impact;                        /* econ_impact_t                     */
    int64_t ts;                            /* event time, UTC epoch (sort key)  */
} econ_event_t;

typedef struct {
    int          count;                    /* events in items[] (<= ECON_EVENT_MAX) */
    int          total_matched;            /* events passing the filter (may exceed count) */
    econ_event_t items[ECON_EVENT_MAX];    /* earliest `count`, sorted ascending by ts */
    char         week_label[ECON_LABEL_MAXLEN];
    bool         valid;                    /* fetch + parse succeeded            */
    char         error[ECON_ERR_MAXLEN];   /* human-readable, set when !valid    */
} econ_calendar_t;
