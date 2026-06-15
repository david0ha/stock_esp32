/*
 * econ_parse.h — pure JSON/time helpers for the economic calendar.
 *
 * No network, no globals: unit-tested on the host against captured FMP JSON.
 * The firmware and the desktop simulator compile this exact file.
 */
#pragma once

#include "econ_model.h"
#include <time.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UTC civil time -> Unix epoch seconds. Independent of libc timegm() so the
 * result is identical on every host and on the device. */
int64_t econ_ymd_to_epoch(int year, int month, int day, int hour, int min, int sec);

/* Map FMP "Low"/"Medium"/"High" (case-insensitive) to an econ_impact_t rank;
 * anything else (NULL, "", "None") -> ECON_IMPACT_NONE. */
int econ_impact_from_str(const char *s);

/* Device-local UTC offset (seconds east of UTC) for the moment `now`, from the
 * configured TZ. Computed by differencing localtime vs gmtime — ESP-IDF newlib's
 * struct tm has no tm_gmtoff. Used to render the calendar / compute "this week"
 * in local time. */
long econ_local_tz_off(time_t now);

/* Compute the Monday..Sunday week containing (now_utc shifted into the device
 * timezone by tz_off seconds), then moved by `week_offset` weeks (0 = current,
 * -1 = previous, +1 = next). Writes ISO "YYYY-MM-DD" bounds to from/to and a
 * short "MM-DD ~ MM-DD" string to label. */
void econ_week_range(time_t now_utc, long tz_off, int week_offset,
                     char *from, size_t from_sz,
                     char *to,   size_t to_sz,
                     char *label, size_t label_sz);

/* Parse an FMP economics-calendar JSON array into `out`: keep events with
 * impact >= min_impact, convert times to device-local (tz_off), sort ascending
 * by time and keep the earliest ECON_EVENT_MAX (total_matched records the full
 * count). Returns 0 on success (out->valid = true). Returns -1 on malformed
 * JSON or an FMP error object (out->valid = false, out->error set; FMP's
 * "Error Message" is surfaced verbatim). Does not set out->week_label. */
int econ_parse_calendar(const char *json, long tz_off, int min_impact,
                        econ_calendar_t *out);

#ifdef __cplusplus
}
#endif
