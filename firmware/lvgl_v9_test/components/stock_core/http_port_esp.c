/*
 * http_port_esp.c — device HTTP port (esp_http_client + TLS cert bundle).
 *
 * Implements http_get() for the firmware. Mirrors the simulator's libcurl
 * port. The response body is accumulated into PSRAM because Finnhub's
 * metric=all payload is ~240KB — far too large for internal RAM.
 */
#include "http_port.h"

#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define HTTP_MAX_RESP (320 * 1024)   /* hard cap; metric=all is ~240KB */

static const char *TAG = "http";

typedef struct { char *buf; size_t len; size_t cap; bool oom; } acc_t;

static esp_err_t on_evt(esp_http_client_event_t *e) {
    if (e->event_id != HTTP_EVENT_ON_DATA) return ESP_OK;
    acc_t *a = (acc_t *)e->user_data;
    if (a->oom) return ESP_OK;

    if (a->len + e->data_len + 1 > a->cap) {
        size_t ncap = a->cap ? a->cap : 8192;
        while (ncap < a->len + e->data_len + 1) ncap *= 2;
        if (ncap > HTTP_MAX_RESP) { a->oom = true; ESP_LOGW(TAG, "response > cap"); return ESP_OK; }
        char *p = heap_caps_realloc(a->buf, ncap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!p) { a->oom = true; ESP_LOGE(TAG, "PSRAM OOM"); return ESP_OK; }
        a->buf = p; a->cap = ncap;
    }
    memcpy(a->buf + a->len, e->data, e->data_len);
    a->len += e->data_len;
    a->buf[a->len] = '\0';
    return ESP_OK;
}

char *http_get(const char *url, int *out_status) {
    if (out_status) *out_status = 0;

    acc_t a = {0};
    esp_http_client_config_t cfg = {
        .url               = url,
        .event_handler     = on_evt,
        .user_data         = &a,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 15000,
        .buffer_size       = 4096,
        .user_agent        = "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) "
                             "AppleWebKit/537.36 (KHTML, like Gecko) "
                             "Chrome/120 Safari/537.36",
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) { free(a.buf); return NULL; }

    esp_err_t err = esp_http_client_perform(c);
    int code = esp_http_client_get_status_code(c);
    esp_http_client_cleanup(c);

    if (out_status) *out_status = code;
    if (err != ESP_OK || a.oom) {
        if (err != ESP_OK) ESP_LOGW(TAG, "GET failed: %s", esp_err_to_name(err));
        free(a.buf);
        return NULL;
    }
    return a.buf;   /* caller frees */
}
