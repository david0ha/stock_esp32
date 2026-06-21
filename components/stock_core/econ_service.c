/*
 * econ_service.c — builds the FMP economics-calendar URL, fetches via the
 * http_get port, and parses into econ_calendar_t. Shared by firmware + simulator.
 *
 * Unlike stock_service (which drops non-200 bodies), this keeps the body on an
 * error status so FMP's "Error Message" can be shown on screen verbatim.
 */
#include "econ_service.h"
#include "econ_parse.h"
#include "http_port.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Source is swappable via Kconfig: FMP by default, or the bundled free
 * investing.com proxy (tools/econ_proxy). Both answer the same
 * ?from=&to=&apikey= query. Fallback keeps the simulator / host tests (no
 * sdkconfig) compiling. */
#ifndef CONFIG_STOCK_ECON_BASE_URL
#define CONFIG_STOCK_ECON_BASE_URL "https://financialmodelingprep.com/stable/economic-calendar"
#endif

#define URL_MAX 320

/* Pull a human-readable reason out of an FMP error body. FMP returns either a
 * JSON object ({"Error Message": "..."}) or a plain-text sentence (e.g. the
 * "Restricted Endpoint ..." paywall reply); surface whichever it is, falling
 * back to the bare HTTP status. snprintf truncates to fit the on-screen field. */
static void error_from_body(char *dst, size_t n, const char *body, int status) {
    if (body && body[0]) {
        cJSON *root = cJSON_Parse(body);
        if (root) {
            const cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "Error Message");
            if (cJSON_IsString(msg) && msg->valuestring[0]) {       /* {"Error Message":...} */
                snprintf(dst, n, "%s", msg->valuestring);
                cJSON_Delete(root);
                return;
            }
            if (cJSON_IsString(root) && root->valuestring[0]) {     /* a bare JSON string */
                snprintf(dst, n, "%s", root->valuestring);
                cJSON_Delete(root);
                return;
            }
            cJSON_Delete(root);
        } else if (body[0] != '[' && body[0] != '{') {   /* plain-text message */
            snprintf(dst, n, "HTTP %d: %s", status, body);
            return;
        }
    }
    snprintf(dst, n, "HTTP %d", status);
}

int econ_service_fetch(const char *fmp_key, const char *base_url, time_t now_utc, long tz_off,
                       int week_offset, int min_impact, econ_calendar_t *out) {
    char from[12], to[12], label[ECON_LABEL_MAXLEN];
    econ_week_range(now_utc, tz_off, week_offset,
                    from, sizeof from, to, sizeof to, label, sizeof label);

    int ret = 0;
    if (!fmp_key || !*fmp_key) {
        memset(out, 0, sizeof(*out));
        snprintf(out->error, sizeof(out->error), "no FMP API key set");
    } else {
        const char *base = (base_url && *base_url) ? base_url : CONFIG_STOCK_ECON_BASE_URL;
        char url[URL_MAX];
        snprintf(url, sizeof(url), "%s?from=%s&to=%s&apikey=%s",
                 base, from, to, fmp_key);

        int st = 0;
        char *body = http_get(url, &st);
        if (!body) {
            memset(out, 0, sizeof(*out));
            snprintf(out->error, sizeof(out->error), "network error");
        } else if (st == 200) {
            if (econ_parse_calendar(body, tz_off, min_impact, out) == 0) ret = 1;
            /* on parse failure econ_parse_calendar has set out->error */
        } else {
            memset(out, 0, sizeof(*out));
            error_from_body(out->error, sizeof(out->error), body, st);
        }
        free(body);
    }

    /* Always name the week — even on the error screen. Set last so it survives
     * econ_parse_calendar's memset of `out`. */
    snprintf(out->week_label, sizeof(out->week_label), "%s", label);
    return ret;
}
