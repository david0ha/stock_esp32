#include "prov_portal.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#include "prov_wifi.h"
#include "form_parse.h"
#include "json_build.h"

static const char *TAG = "prov_portal";

// Embedded setup page (see portal.html). EMBED_TXTFILES NUL-terminates it.
extern const char portal_html_start[] asm("_binary_portal_html_start");

static prov_config_t          s_current;
static bool                   s_have_current;
static prov_portal_save_cb_t  s_on_save;
static void                  *s_user;

// ---------------------------------------------------------------------------
// HTTP handlers
// ---------------------------------------------------------------------------

static esp_err_t index_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s (serving portal page)", req->uri);
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, portal_html_start, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t scan_get(httpd_req_t *req)
{
    static prov_ap_t aps[24];
    // Serve the background-scan cache; scanning live here would drop the connected client.
    size_t count = prov_wifi_scan_cached(aps, sizeof(aps) / sizeof(aps[0]));
    ESP_LOGI(TAG, "/scan -> %u network(s)", (unsigned)count);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"networks\":[");
    for (size_t i = 0; i < count; i++) {
        char esc[PROV_JSON_ESCAPE_BUFSZ(sizeof(aps[0].ssid))];
        if (prov_json_escape(aps[i].ssid, esc, sizeof(esc)) < 0) {
            ESP_LOGW(TAG, "skipped SSID that overflowed JSON escape buffer");
            continue;
        }
        char obj[256];
        snprintf(obj, sizeof(obj), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
                 i == 0 ? "" : ",", esc, aps[i].rssi, aps[i].secure ? "true" : "false");
        httpd_resp_sendstr_chunk(req, obj);
    }
    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_sendstr_chunk(req, NULL);  // end response
    return ESP_OK;
}

static esp_err_t state_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /state");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr_chunk(req, "{\"ssid\":\"");
    if (s_have_current) {
        char esc[PROV_JSON_ESCAPE_BUFSZ(sizeof(s_current.ssid))];
        if (prov_json_escape(s_current.ssid, esc, sizeof(esc)) >= 0) {
            httpd_resp_sendstr_chunk(req, esc);
        }
    }
    httpd_resp_sendstr_chunk(req, "\",\"tickers\":[");
    for (size_t i = 0; s_have_current && i < s_current.ticker_count; i++) {
        char esc[PROV_JSON_ESCAPE_BUFSZ(sizeof(s_current.tickers[0]))];
        if (prov_json_escape(s_current.tickers[i], esc, sizeof(esc)) < 0) {
            continue;
        }
        char obj[sizeof(esc) + 4];  // esc + leading comma + two quotes + NUL
        snprintf(obj, sizeof(obj), "%s\"%s\"", i == 0 ? "" : ",", esc);
        httpd_resp_sendstr_chunk(req, obj);
    }
    httpd_resp_sendstr_chunk(req, "]}");
    httpd_resp_sendstr_chunk(req, NULL);
    return ESP_OK;
}

static esp_err_t save_post(httpd_req_t *req)
{
    char body[1024];
    if (req->content_len > sizeof(body) - 1) {
        // The legitimate form is a few hundred bytes; reject anything larger rather than
        // silently truncating (which would leave unread bytes in the stream).
        httpd_resp_set_status(req, "413 Payload Too Large");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"Request too large.\"}");
    }
    int total = 0;
    int remaining = req->content_len;
    while (remaining > 0) {
        int r = httpd_req_recv(req, body + total, remaining);
        if (r <= 0) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "read error");
        }
        total += r;
        remaining -= r;
    }
    body[total] = '\0';

    prov_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    char ssid[64] = {0};
    char password[96] = {0};
    char tickers[256] = {0};
    prov_form_get_field(body, "ssid", ssid, sizeof(ssid));
    prov_form_get_field(body, "password", password, sizeof(password));
    prov_form_get_field(body, "tickers", tickers, sizeof(tickers));

    if (ssid[0] == '\0') {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"Network name is required.\"}");
    }
    // Reject over-length credentials rather than silently truncating them into cfg
    // (a truncated SSID/passphrase would just fail to connect, confusingly).
    if (strlen(ssid) > PROV_SSID_MAX_LEN) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"Network name is too long.\"}");
    }
    if (strlen(password) > PROV_PASS_MAX_LEN) {
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"ok\":false,\"error\":\"Password is too long.\"}");
    }

    strlcpy(cfg.ssid, ssid, sizeof(cfg.ssid));
    strlcpy(cfg.password, password, sizeof(cfg.password));
    prov_tickers_parse(&cfg, tickers);

    ESP_LOGI(TAG, "config submitted: ssid='%s', %u tickers",
             cfg.ssid, (unsigned)cfg.ticker_count);

    // Respond before triggering the save/reboot so the page receives confirmation.
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"ok\":true}");

    if (s_on_save) {
        s_on_save(&cfg, s_user);
    }
    return ESP_OK;
}

// Redirect every unknown URL (OS captive-detection probes) to the portal root.
static esp_err_t captive_redirect(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    ESP_LOGI(TAG, "captive redirect: %s %s -> /",
             req->method == HTTP_GET ? "GET" : "?", req->uri);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, "redirect", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static void start_http(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 8;
    config.lru_purge_enable = true;
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return;
    }

    const httpd_uri_t routes[] = {
        {.uri = "/",      .method = HTTP_GET,  .handler = index_get},
        {.uri = "/scan",  .method = HTTP_GET,  .handler = scan_get},
        {.uri = "/state", .method = HTTP_GET,  .handler = state_get},
        {.uri = "/save",  .method = HTTP_POST, .handler = save_post},
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_redirect);
}

// ---------------------------------------------------------------------------
// DNS hijack: answer every A query with the SoftAP IP so the OS opens the portal.
// ---------------------------------------------------------------------------

static void dns_task(void *arg)
{
    (void)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "dns socket failed");
        vTaskDelete(NULL);
        return;
    }
    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(53);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        ESP_LOGE(TAG, "dns bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    uint8_t pkt[512];
    while (1) {
        struct sockaddr_in client;
        socklen_t clen = sizeof(client);
        int n = recvfrom(sock, pkt, sizeof(pkt), 0, (struct sockaddr *)&client, &clen);
        if (n < 12) {
            continue;  // smaller than a DNS header
        }

        pkt[2] = 0x81;  // QR=1, recursion desired preserved-ish
        pkt[3] = 0x80;  // RA=1, RCODE=0
        pkt[6] = 0x00;  // ANCOUNT hi
        pkt[7] = 0x01;  // ANCOUNT lo = 1
        pkt[8] = 0x00;  // NSCOUNT
        pkt[9] = 0x00;
        pkt[10] = 0x00;  // ARCOUNT (drop any EDNS OPT)
        pkt[11] = 0x00;

        // Walk the question's QNAME labels to find where the answer should start.
        int q = 12;
        while (q < n && pkt[q] != 0) {
            q += pkt[q] + 1;
        }
        q += 1;  // terminating zero label
        q += 4;  // QTYPE + QCLASS
        if (q <= 12 || q + 16 > (int)sizeof(pkt) || q > n) {
            continue;  // malformed / oversized
        }

        int p = q;
        pkt[p++] = 0xC0; pkt[p++] = 0x0C;             // name -> pointer to offset 12
        pkt[p++] = 0x00; pkt[p++] = 0x01;             // TYPE A
        pkt[p++] = 0x00; pkt[p++] = 0x01;             // CLASS IN
        pkt[p++] = 0x00; pkt[p++] = 0x00;
        pkt[p++] = 0x00; pkt[p++] = 0x1C;             // TTL 28s
        pkt[p++] = 0x00; pkt[p++] = 0x04;             // RDLENGTH 4
        pkt[p++] = 192; pkt[p++] = 168; pkt[p++] = 4; pkt[p++] = 1;  // 192.168.4.1

        sendto(sock, pkt, p, 0, (struct sockaddr *)&client, clen);
    }
}

// ---------------------------------------------------------------------------

void prov_portal_start(const prov_config_t *current, prov_portal_save_cb_t on_save, void *user)
{
    s_have_current = (current != NULL);
    if (current != NULL) {
        s_current = *current;
    }
    s_on_save = on_save;
    s_user = user;

    start_http();
    xTaskCreate(dns_task, "prov_dns", 4096, NULL, 5, NULL);
}
