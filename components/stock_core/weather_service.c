/*
 * weather_service.c — see weather_service.h. Builds the keyless Open-Meteo URLs,
 * fetches via the http_get platform seam, and parses with weather_parse.
 */
#include "weather_service.h"
#include "weather_parse.h"
#include "http_port.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GEO_BASE  "https://geocoding-api.open-meteo.com/v1/search"
#define FCST_BASE "https://api.open-meteo.com/v1/forecast"
#define URL_MAX   320

/* Percent-encode a free-text query into `dst` for use as a URL query value.
 * Keeps unreserved characters; everything else (space, comma, UTF-8 bytes) is
 * %XX-escaped. Always NUL-terminates and never overflows `dst`. */
static void url_encode(const char *src, char *dst, size_t dst_sz) {
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && o + 4 < dst_sz; p++) {
        unsigned char c = *p;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            dst[o++] = (char)c;
        } else {
            dst[o++] = '%';
            dst[o++] = hex[c >> 4];
            dst[o++] = hex[c & 0x0F];
        }
    }
    dst[o] = '\0';
}

int weather_service_geocode(const char *query, geo_loc_t *out) {
    if (out) out->valid = false;
    if (!query || !*query || !out) return 0;

    char enc[160];
    url_encode(query, enc, sizeof(enc));

    char url[URL_MAX];
    snprintf(url, sizeof(url), "%s?name=%s&count=1&language=en&format=json",
             GEO_BASE, enc);

    int st = 0;
    char *body = http_get(url, &st);
    int ok = 0;
    if (body && st == 200 && geo_parse(body, out) == 0) ok = 1;
    free(body);
    return ok;
}

int weather_service_fetch(double lat, double lon, weather_t *out) {
    if (out) out->valid = false;
    if (!out) return 0;

    char url[URL_MAX];
    snprintf(url, sizeof(url),
             "%s?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,weather_code"
             "&daily=weather_code,temperature_2m_max,temperature_2m_min"
             "&timezone=auto&forecast_days=%d",
             FCST_BASE, lat, lon, WX_FORECAST_MAX);

    int st = 0;
    char *body = http_get(url, &st);
    int ok = 0;
    if (body && st == 200 && weather_parse(body, out) == 0) ok = 1;
    free(body);
    return ok;
}
