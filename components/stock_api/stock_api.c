#include "stock_api.h"

#include <stdio.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "mdns.h"

#include "cJSON.h"
#include "stock_api_json.h"
#include "user_app_control.h"

static const char *TAG = "stock_api";

// State JSON for a full 16-symbol watchlist is ~2KB; size with headroom.
#define STATE_BUF_SZ 2800
// Control bodies are tiny ({"index":3} etc.); a watchlist of 16 symbols is the largest.
#define POST_BUF_SZ  512

// ---------------------------------------------------------------------------
// Small response helpers
// ---------------------------------------------------------------------------

static esp_err_t send_json(httpd_req_t *req, const char *status, const char *body)
{
    if (status != NULL) {
        httpd_resp_set_status(req, status);
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");
    // Friendly to a browser-based dev build (react-native-web); native RN does not need it.
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, body);
}

static esp_err_t send_ok(httpd_req_t *req)
{
    return send_json(req, NULL, "{\"ok\":true}");
}

// 400 with {"ok":false,"error":"<code>"} — the app maps `error` to a typed Esp32Error.
static esp_err_t send_err(httpd_req_t *req, const char *code)
{
    char buf[96];
    snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", code);
    return send_json(req, "400 Bad Request", buf);
}

// Read the full request body into `buf` (NUL-terminated). Returns the byte count, or <0 on an
// oversize body / socket error (caller replies bad_json/too_large).
static int read_body(httpd_req_t *req, char *buf, size_t bufsz)
{
    if (req->content_len > (int)bufsz - 1) {
        return -1;
    }
    int total = 0;
    int remaining = req->content_len;
    while (remaining > 0) {
        int r = httpd_req_recv(req, buf + total, remaining);
        if (r == HTTPD_SOCK_ERR_TIMEOUT) {
            continue;   // transient recv timeout/interrupt — retry rather than abort the body
        }
        if (r <= 0) {
            return -2;
        }
        total += r;
        remaining -= r;
    }
    buf[total] = '\0';
    return total;
}

// ---------------------------------------------------------------------------
// Device identity
// ---------------------------------------------------------------------------

static void device_id(char *out /* >= 5 bytes */)
{
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, 5, "%02X%02X", mac[4], mac[5]);
}

static void sta_ip(char *out, size_t n)
{
    out[0] = '\0';
    esp_netif_t *nif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (nif == NULL) {
        return;
    }
    esp_netif_ip_info_t ip;
    if (esp_netif_get_ip_info(nif, &ip) == ESP_OK) {
        snprintf(out, n, IPSTR, IP2STR(&ip.ip));
    }
}

// ---------------------------------------------------------------------------
// GET handlers
// ---------------------------------------------------------------------------

static esp_err_t api_info_get(httpd_req_t *req)
{
    char id[STOCK_API_DEVID_MAXLEN], ip[STOCK_API_IP_MAXLEN];
    device_id(id);
    sta_ip(ip, sizeof(ip));
    char body[256];
    if (stock_api_json_info(body, sizeof(body), id, STOCK_APP_MODEL, STOCK_APP_FW, ip) < 0) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "info");
    }
    return send_json(req, NULL, body);
}

static esp_err_t api_state_get(httpd_req_t *req)
{
    stock_api_state_t st;
    user_app_snapshot(&st);            // fills model/fw/index/page/econ/env/watchlist
    device_id(st.device_id);           // network identity is owned here, not by user_app
    sta_ip(st.ip, sizeof(st.ip));

    static char buf[STATE_BUF_SZ];     // the single httpd task serializes one response at a time
    if (stock_api_json_state(&st, buf, sizeof(buf)) < 0) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "state");
    }
    return send_json(req, NULL, buf);
}

// ---------------------------------------------------------------------------
// POST handlers — parse a small JSON body, drive the app via user_app_control
// ---------------------------------------------------------------------------

// Read + parse the JSON body. On success returns the cJSON root (caller must cJSON_Delete) and
// leaves *err NULL; on failure returns NULL and sends the error response itself.
static cJSON *parse_body(httpd_req_t *req, esp_err_t *sent)
{
    char body[POST_BUF_SZ];
    int blen = read_body(req, body, sizeof(body));
    if (blen == -1) { *sent = send_err(req, "too_large"); return NULL; }
    if (blen < 0)   { *sent = send_err(req, "read_error"); return NULL; }
    cJSON *root = cJSON_Parse(body);
    if (root == NULL) { *sent = send_err(req, "bad_json"); return NULL; }
    return root;
}

static esp_err_t api_select_post(httpd_req_t *req)
{
    esp_err_t sent;
    cJSON *root = parse_body(req, &sent);
    if (root == NULL) return sent;

    esp_err_t rc;
    cJSON *idx = cJSON_GetObjectItem(root, "index");
    cJSON *sym = cJSON_GetObjectItem(root, "symbol");
    if (cJSON_IsNumber(idx)) {
        rc = user_app_select_index((int)idx->valuedouble) ? send_ok(req) : send_err(req, "index_range");
    } else if (cJSON_IsString(sym) && sym->valuestring != NULL) {
        rc = user_app_select_symbol(sym->valuestring) ? send_ok(req) : send_err(req, "symbol_not_found");
    } else {
        rc = send_err(req, "bad_json");
    }
    cJSON_Delete(root);
    return rc;
}

static esp_err_t api_page_post(httpd_req_t *req)
{
    esp_err_t sent;
    cJSON *root = parse_body(req, &sent);
    if (root == NULL) return sent;

    esp_err_t rc;
    cJSON *pg = cJSON_GetObjectItem(root, "page");
    if (!cJSON_IsNumber(pg)) {
        rc = send_err(req, "bad_json");
    } else {
        rc = user_app_set_page((int)pg->valuedouble) ? send_ok(req) : send_err(req, "page_range");
    }
    cJSON_Delete(root);
    return rc;
}

static esp_err_t api_econ_post(httpd_req_t *req)
{
    esp_err_t sent;
    cJSON *root = parse_body(req, &sent);
    if (root == NULL) return sent;

    esp_err_t rc;
    cJSON *mode = cJSON_GetObjectItem(root, "mode");
    if (!cJSON_IsBool(mode)) {
        rc = send_err(req, "bad_json");
    } else {
        int week = 0;
        cJSON *wk = cJSON_GetObjectItem(root, "week");
        if (cJSON_IsNumber(wk)) week = (int)wk->valuedouble;
        user_app_set_econ(cJSON_IsTrue(mode), week);
        rc = send_ok(req);
    }
    cJSON_Delete(root);
    return rc;
}

static esp_err_t api_refresh_post(httpd_req_t *req)
{
    esp_err_t sent;
    cJSON *root = parse_body(req, &sent);
    if (root == NULL) return sent;

    cJSON *all = cJSON_GetObjectItem(root, "all");
    user_app_refresh(cJSON_IsTrue(all));   // missing/!bool => refresh current only
    cJSON_Delete(root);
    return send_ok(req);
}

// Count comma/space/tab/newline-separated tokens, matching prov_tickers_parse's splitting, so
// the 1..16 cap is enforced identically for both the array and string body forms.
static int count_tokens(const char *csv)
{
    int n = 0;
    bool in_tok = false;
    for (const char *p = csv; *p; p++) {
        bool sep = (*p == ',' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r');
        if (sep) {
            in_tok = false;
        } else if (!in_tok) {
            n++;
            in_tok = true;
        }
    }
    return n;
}

static esp_err_t api_watchlist_post(httpd_req_t *req)
{
    esp_err_t sent;
    cJSON *root = parse_body(req, &sent);
    if (root == NULL) return sent;

    cJSON *t = cJSON_GetObjectItem(root, "tickers");
    char csv[256];
    csv[0] = '\0';

    if (cJSON_IsString(t) && t->valuestring != NULL) {
        strlcpy(csv, t->valuestring, sizeof(csv));        // already a comma/space list
    } else if (cJSON_IsArray(t)) {
        size_t len = 0;
        cJSON *e = NULL;
        cJSON_ArrayForEach(e, t) {
            if (!cJSON_IsString(e) || e->valuestring == NULL) continue;   // skip non-strings
            int w = snprintf(csv + len, sizeof(csv) - len, "%s%s", len ? "," : "", e->valuestring);
            if (w < 0 || (size_t)w >= sizeof(csv) - len) break;
            len += (size_t)w;
        }
    } else {
        cJSON_Delete(root);
        return send_err(req, "bad_json");
    }

    // Enforce the cap uniformly for BOTH body forms (the string form previously bypassed it), and
    // against usable tokens only — a JSON array padded with non-strings (skipped above) won't
    // falsely trip it. Matches the mock and docs/app-control.md (1..16).
    esp_err_t rc;
    if (count_tokens(csv) > STOCK_API_MAX_TICKERS) {
        rc = send_err(req, "too_many_tickers");
    } else {
        // user_app_set_watchlist normalizes/dedupes and returns the resulting count (0 = empty).
        rc = (user_app_set_watchlist(csv) > 0) ? send_ok(req) : send_err(req, "empty_watchlist");
    }
    cJSON_Delete(root);
    return rc;
}

// POST /api/stock/keys { finnhubKey?, fmpKey?, econUrl? } — update the runtime data-source keys
// live (persisted to NVS). Each present string is applied (empty clears -> Kconfig default); an
// absent field is left unchanged. The values are never read back (GET state reports presence only).
static esp_err_t api_keys_post(httpd_req_t *req)
{
    esp_err_t sent;
    cJSON *root = parse_body(req, &sent);
    if (root == NULL) return sent;

    cJSON *fh  = cJSON_GetObjectItem(root, "finnhubKey");
    cJSON *fmp = cJSON_GetObjectItem(root, "fmpKey");
    cJSON *eu  = cJSON_GetObjectItem(root, "econUrl");
    const char *p_fh  = cJSON_IsString(fh)  ? fh->valuestring  : NULL;
    const char *p_fmp = cJSON_IsString(fmp) ? fmp->valuestring : NULL;
    const char *p_eu  = cJSON_IsString(eu)  ? eu->valuestring  : NULL;

    // user_app_set_keys returns false only when no recognized field was provided.
    esp_err_t rc = user_app_set_keys(p_fh, p_fmp, p_eu) ? send_ok(req) : send_err(req, "bad_json");
    cJSON_Delete(root);
    return rc;
}

// POST /api/stock/location { location } — set the weather location live (persisted to NVS; the
// device re-geocodes it via Open-Meteo). An empty string turns the weather widget off.
static esp_err_t api_location_post(httpd_req_t *req)
{
    esp_err_t sent;
    cJSON *root = parse_body(req, &sent);
    if (root == NULL) return sent;

    cJSON *loc = cJSON_GetObjectItem(root, "location");
    esp_err_t rc;
    if (!cJSON_IsString(loc) || loc->valuestring == NULL) {
        rc = send_err(req, "bad_json");
    } else {
        rc = user_app_set_location(loc->valuestring) ? send_ok(req) : send_err(req, "bad_json");
    }
    cJSON_Delete(root);
    return rc;
}

// ---------------------------------------------------------------------------
// Server + mDNS bring-up
// ---------------------------------------------------------------------------

static void start_http(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 12;
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return;
    }

    const httpd_uri_t routes[] = {
        {.uri = "/api/info",            .method = HTTP_GET,  .handler = api_info_get},
        {.uri = "/api/stock/state",     .method = HTTP_GET,  .handler = api_state_get},
        {.uri = "/api/stock/select",    .method = HTTP_POST, .handler = api_select_post},
        {.uri = "/api/stock/page",      .method = HTTP_POST, .handler = api_page_post},
        {.uri = "/api/stock/econ",      .method = HTTP_POST, .handler = api_econ_post},
        {.uri = "/api/stock/refresh",   .method = HTTP_POST, .handler = api_refresh_post},
        {.uri = "/api/stock/watchlist", .method = HTTP_POST, .handler = api_watchlist_post},
        {.uri = "/api/stock/keys",      .method = HTTP_POST, .handler = api_keys_post},
        {.uri = "/api/stock/location",  .method = HTTP_POST, .handler = api_location_post},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
    ESP_LOGI(TAG, "control server up on port 80");
}

static void start_mdns(void)
{
    esp_err_t err = mdns_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "mdns_init failed: %s", esp_err_to_name(err));
        return;
    }
    mdns_hostname_set("tickerboard");          // -> tickerboard.local
    mdns_instance_name_set("Ticker Board");
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    ESP_LOGI(TAG, "mDNS advertising http://tickerboard.local");
}

void stock_api_start(void)
{
    start_http();
    start_mdns();
}
