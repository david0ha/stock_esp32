/*
 * Host unit tests for the econ_service layer.
 *
 * econ_service talks to the network only through the http_get() seam, so we link
 * a fake http_get here (no sockets) and drive econ_service_fetch against canned
 * responses + statuses. Builds with cmake (see CMakeLists.txt).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "econ_service.h"
#include "econ_parse.h"
#include "http_port.h"

static int g_total = 0, g_fail = 0;

#define CHECK(cond) do { g_total++; if (!(cond)) { g_fail++; \
    printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_STR(a, b) do { g_total++; if (strcmp((a), (b)) != 0) { g_fail++; \
    printf("  FAIL %s:%d  %s == \"%s\"  got \"%s\"\n", __FILE__, __LINE__, #a, (b), (a)); } } while (0)

/* ---- fake http_get: captures the URL, returns a configured body + status ---- */
static const char *g_next_body;
static int         g_next_status;
static char        g_last_url[512];

char *http_get(const char *url, int *out_status) {
    snprintf(g_last_url, sizeof(g_last_url), "%s", url ? url : "");
    if (out_status) *out_status = g_next_status;
    if (!g_next_body) return NULL;
    return strdup(g_next_body);
}

#define KST (9L * 3600L)

static void test_success(void) {
    printf("test_success\n");
    static const char *BODY =
        "[{\"date\":\"2026-06-17 03:00:00\",\"country\":\"JP\","
        "\"event\":\"BoJ Rate\",\"estimate\":0.75,\"actual\":1.0,"
        "\"previous\":0.75,\"impact\":\"High\"}]";
    g_next_body = BODY; g_next_status = 200;

    econ_calendar_t c;
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);
    int rc = econ_service_fetch("KEY", now, 0, 0, ECON_IMPACT_HIGH, &c);
    CHECK(rc == 1);
    CHECK(c.valid);
    CHECK(c.count == 1);
    CHECK_STR(c.week_label, "06-15 ~ 06-21");

    /* URL must hit the stable economic-calendar endpoint with the week bounds + key */
    CHECK(strstr(g_last_url, "/stable/economic-calendar") != NULL);
    CHECK(strstr(g_last_url, "from=2026-06-15") != NULL);
    CHECK(strstr(g_last_url, "to=2026-06-21") != NULL);
    CHECK(strstr(g_last_url, "apikey=KEY") != NULL);
}

static void test_week_offset_in_url(void) {
    printf("test_week_offset_in_url\n");
    g_next_body = "[]"; g_next_status = 200;
    econ_calendar_t c;
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);

    econ_service_fetch("KEY", now, 0, -1, ECON_IMPACT_HIGH, &c);
    CHECK(strstr(g_last_url, "from=2026-06-08") != NULL);
    CHECK(strstr(g_last_url, "to=2026-06-14") != NULL);
    CHECK_STR(c.week_label, "06-08 ~ 06-14");
}

static void test_missing_key(void) {
    printf("test_missing_key\n");
    econ_calendar_t c;
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);
    int rc = econ_service_fetch("", now, 0, 0, ECON_IMPACT_HIGH, &c);
    CHECK(rc == 0);
    CHECK(!c.valid);
    CHECK(c.error[0] != '\0');
    CHECK_STR(c.week_label, "06-15 ~ 06-21");   /* week still named on the error screen */
}

static void test_transport_failure(void) {
    printf("test_transport_failure\n");
    g_next_body = NULL; g_next_status = 0;
    econ_calendar_t c;
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);
    int rc = econ_service_fetch("KEY", now, 0, 0, ECON_IMPACT_HIGH, &c);
    CHECK(rc == 0);
    CHECK(!c.valid);
    CHECK(c.error[0] != '\0');
}

static void test_http_error_status(void) {
    printf("test_http_error_status\n");
    /* FMP returns the error as a JSON body even on a non-200 status. */
    g_next_body = "{\"Error Message\":\"Limit Reach. Upgrade your plan.\"}";
    g_next_status = 403;
    econ_calendar_t c;
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);
    int rc = econ_service_fetch("KEY", now, 0, 0, ECON_IMPACT_HIGH, &c);
    CHECK(rc == 0);
    CHECK(!c.valid);
    CHECK(strstr(c.error, "Limit Reach") != NULL);
}

static void test_plaintext_error_body(void) {
    printf("test_plaintext_error_body\n");
    /* FMP's "restricted plan" reply is plain text, not JSON — surface it (with
     * the status) rather than a bare "HTTP 402". */
    g_next_body = "Restricted Endpoint: This endpoint is not available under "
                  "your current subscription";
    g_next_status = 402;
    econ_calendar_t c;
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);
    int rc = econ_service_fetch("KEY", now, 0, 0, ECON_IMPACT_HIGH, &c);
    CHECK(rc == 0);
    CHECK(!c.valid);
    CHECK(strstr(c.error, "Restricted Endpoint") != NULL);
}

static void test_fmp_error_200(void) {
    printf("test_fmp_error_200\n");
    /* Some FMP errors come back with status 200 + an Error Message object. */
    g_next_body = "{\"Error Message\":\"Invalid API KEY.\"}";
    g_next_status = 200;
    econ_calendar_t c;
    time_t now = (time_t)econ_ymd_to_epoch(2026, 6, 16, 12, 0, 0);
    int rc = econ_service_fetch("KEY", now, 0, 0, ECON_IMPACT_HIGH, &c);
    CHECK(rc == 0);
    CHECK(!c.valid);
    CHECK(strstr(c.error, "Invalid API KEY") != NULL);
}

int main(void) {
    test_success();
    test_week_offset_in_url();
    test_missing_key();
    test_transport_failure();
    test_http_error_status();
    test_plaintext_error_body();
    test_fmp_error_200();
    printf("\n%s  (%d checks, %d failed)\n", g_fail ? "FAILED" : "OK", g_total, g_fail);
    return g_fail ? 1 : 0;
}
