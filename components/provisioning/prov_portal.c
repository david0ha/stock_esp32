#include "prov_portal.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "lwip/sockets.h"

#include "prov_wifi.h"
#include "form_parse.h"

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

// Force single-use connections. The iOS Captive Network Assistant (and Android's
// CaptivePortalLogin) runs the portal inside a sandboxed WebView that is unreliable about
// reusing an HTTP/1.1 keep-alive socket for its follow-up XHRs. Combined with the server's
// lru_purge reclaiming idle sockets, the browser's fetch('/scan')/fetch('/state') ended up
// written onto a connection the server had already moved on from, so those requests never
// reached a handler (the dropdown hung on "Scanning…"). Sending "Connection: close" on every
// response makes the client open a fresh connection per request — the same mitigation
// xiaozhi's esp-wifi-connect applies to all of its handlers. Must be set before the first
// send/chunk; the literals have static storage so the queued header stays valid.
static void no_keepalive(httpd_req_t *req)
{
    httpd_resp_set_hdr(req, "Connection", "close");
}

// Escape text for an HTML attribute/text context (&, <, >, ", '). Always NUL-terminates;
// silently stops if `out` fills (callers size `out` for the worst case of 6 bytes/char).
static void html_escape(const char *in, char *out, size_t outsz)
{
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0'; i++) {
        const char *rep = NULL;
        switch (in[i]) {
            case '&':  rep = "&amp;";  break;
            case '<':  rep = "&lt;";   break;
            case '>':  rep = "&gt;";   break;
            case '"':  rep = "&quot;"; break;
            case '\'': rep = "&#39;";  break;
            default: break;
        }
        if (rep != NULL) {
            size_t l = strlen(rep);
            if (o + l >= outsz) break;
            memcpy(out + o, rep, l);
            o += l;
        } else {
            if (o + 1 >= outsz) break;
            out[o++] = in[i];
        }
    }
    out[o] = '\0';
}

// Send the embedded template up to `marker` as a chunk; return the pointer just past it.
// If the marker is absent, flush the remainder and return NULL so the caller stops injecting.
static const char *emit_until(httpd_req_t *req, const char *from, const char *marker)
{
    if (from == NULL) {
        return NULL;
    }
    const char *m = strstr(from, marker);
    if (m == NULL) {
        httpd_resp_sendstr_chunk(req, from);
        return NULL;
    }
    httpd_resp_send_chunk(req, from, m - from);
    return m + strlen(marker);
}

// Server-render the <option> list from the background-scan cache. The captive WebView does
// not reliably run fetch()/XHR, so the network list must be baked into the page rather than
// loaded by a second request. Marks the saved network selected and always appends "Other…".
static void emit_options(httpd_req_t *req)
{
    static prov_ap_t aps[24];
    size_t count = prov_wifi_scan_cached(aps, sizeof(aps) / sizeof(aps[0]));
    ESP_LOGI(TAG, "rendering %u cached network(s) into portal page", (unsigned)count);

    const char *saved = s_have_current ? s_current.ssid : "";
    bool saved_seen = false;

    for (size_t i = 0; i < count; i++) {
        char esc[6 * sizeof(aps[0].ssid) + 1];
        html_escape(aps[i].ssid, esc, sizeof(esc));
        bool sel = (saved[0] != '\0' && strcmp(aps[i].ssid, saved) == 0);
        if (sel) saved_seen = true;
        const char *bars = aps[i].rssi >= -55 ? "●●●" : aps[i].rssi >= -70 ? "●●" : "●";
        char opt[600];
        snprintf(opt, sizeof(opt), "<option value=\"%s\"%s>%s%s   %s</option>",
                 esc, sel ? " selected" : "", esc, aps[i].secure ? " 🔒" : "", bars);
        httpd_resp_sendstr_chunk(req, opt);
    }
    // A previously-saved network that is out of range right now should still appear (selected),
    // so reconfiguring shows the current choice rather than silently dropping it.
    if (saved[0] != '\0' && !saved_seen) {
        char esc[6 * sizeof(s_current.ssid) + 1];
        html_escape(saved, esc, sizeof(esc));
        char opt[600];
        snprintf(opt, sizeof(opt), "<option value=\"%s\" selected>%s   (saved)</option>", esc, esc);
        httpd_resp_sendstr_chunk(req, opt);
    }
    if (count == 0 && saved[0] == '\0') {
        httpd_resp_sendstr_chunk(req, "<option value=\"\">No networks found — tap ⟳ to reload</option>");
    }
    httpd_resp_sendstr_chunk(req, "<option value=\"__manual__\">Other network…</option>");
}

// Fill the {{TICKERS}} slot with the saved watchlist as a comma-separated value (escaped),
// so reconfiguring pre-populates the field. Empty for a fresh setup.
static void emit_saved_tickers(httpd_req_t *req)
{
    if (!s_have_current || s_current.ticker_count == 0) {
        return;
    }
    char csv[PROV_MAX_TICKERS * (PROV_TICKER_MAX_LEN + 1) + 1];
    prov_tickers_serialize(&s_current, csv, sizeof(csv));
    char esc[sizeof(csv) * 6 + 1];
    html_escape(csv, esc, sizeof(esc));
    httpd_resp_sendstr_chunk(req, esc);
}

// Fill the {{LOCATION}} slot with the saved weather location (escaped), so
// reconfiguring pre-populates the field. Empty for a fresh setup.
static void emit_saved_location(httpd_req_t *req)
{
    if (!s_have_current || s_current.location[0] == '\0') {
        return;
    }
    char esc[PROV_LOCATION_MAX_LEN * 6 + 1];
    html_escape(s_current.location, esc, sizeof(esc));
    httpd_resp_sendstr_chunk(req, esc);
}

static esp_err_t index_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET %s (serving portal page)", req->uri);
    no_keepalive(req);
    httpd_resp_set_type(req, "text/html");

    // Stream the template, substituting the server-rendered network list, saved watchlist
    // and saved location at their markers. No client-side fetch is required to populate the form.
    const char *p = portal_html_start;
    p = emit_until(req, p, "{{OPTIONS}}");
    emit_options(req);
    p = emit_until(req, p, "{{TICKERS}}");
    emit_saved_tickers(req);
    p = emit_until(req, p, "{{LOCATION}}");
    emit_saved_location(req);
    if (p != NULL) {
        httpd_resp_sendstr_chunk(req, p);
    }
    httpd_resp_sendstr_chunk(req, NULL);  // end response
    return ESP_OK;
}

// Minimal styled result page. Because the form now does a native POST (a full-page
// navigation, not an XHR), /save must answer with HTML the browser can display.
static esp_err_t send_result_page(httpd_req_t *req, const char *title, const char *msg, bool ok)
{
    no_keepalive(req);
    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr_chunk(req,
        "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        "<title>Ticker Board</title><style>"
        "body{margin:0;min-height:100vh;display:grid;place-items:center;padding:24px;"
        "background:#0b0e14;color:#e8edf4;font-family:system-ui,-apple-system,sans-serif}"
        ".box{max-width:380px;text-align:center}.box h1{font-size:18px;margin:0 0 10px}"
        ".box p{color:#8a93a6;font-size:14px;margin:0 0 18px;line-height:1.5}"
        ".box a{display:inline-block;padding:11px 16px;border-radius:10px;text-decoration:none;"
        "font-weight:600;background:#1fcf8f;color:#06231a}</style></head><body><div class=\"box\">");
    char buf[320];
    snprintf(buf, sizeof(buf), "<h1 style=\"color:%s\">%s</h1><p>%s</p>",
             ok ? "#1fcf8f" : "#ff5c6c", title, msg);
    httpd_resp_sendstr_chunk(req, buf);
    if (!ok) {
        httpd_resp_sendstr_chunk(req, "<a href=\"/\">← Back to setup</a>");
    }
    httpd_resp_sendstr_chunk(req, "</div></body></html>");
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
        return send_result_page(req, "Request too large", "Please go back and try again.", false);
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

    char ssid[64] = {0};
    char ssid_manual[64] = {0};
    char password[96] = {0};
    char tickers[256] = {0};
    char location[128] = {0};
    prov_form_get_field(body, "ssid", ssid, sizeof(ssid));
    prov_form_get_field(body, "ssid_manual", ssid_manual, sizeof(ssid_manual));
    prov_form_get_field(body, "password", password, sizeof(password));
    prov_form_get_field(body, "tickers", tickers, sizeof(tickers));
    prov_form_get_field(body, "location", location, sizeof(location));

    // "Other network…" selected → the real SSID is in the manual field, not the sentinel.
    if (strcmp(ssid, "__manual__") == 0) {
        strlcpy(ssid, ssid_manual, sizeof(ssid));
    }

    if (ssid[0] == '\0') {
        return send_result_page(req, "Network name is required",
                                "Please go back and choose or enter a Wi-Fi network.", false);
    }
    // Reject over-length credentials rather than silently truncating them into cfg
    // (a truncated SSID/passphrase would just fail to connect, confusingly).
    if (strlen(ssid) > PROV_SSID_MAX_LEN) {
        return send_result_page(req, "Network name is too long",
                                "The network name exceeds the 32-character limit.", false);
    }
    if (strlen(password) > PROV_PASS_MAX_LEN) {
        return send_result_page(req, "Password is too long",
                                "The password exceeds the 64-character limit.", false);
    }
    if (strlen(location) > PROV_LOCATION_MAX_LEN) {
        return send_result_page(req, "Location is too long",
                                "The weather location exceeds the 48-character limit.", false);
    }

    prov_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    strlcpy(cfg.ssid, ssid, sizeof(cfg.ssid));
    strlcpy(cfg.password, password, sizeof(cfg.password));
    strlcpy(cfg.location, location, sizeof(cfg.location));
    prov_tickers_parse(&cfg, tickers);

    ESP_LOGI(TAG, "config submitted: ssid='%s', %u tickers, loc='%s'",
             cfg.ssid, (unsigned)cfg.ticker_count, cfg.location);

    char esc[PROV_SSID_MAX_LEN * 6 + 1];
    html_escape(cfg.ssid, esc, sizeof(esc));
    char msg[320];
    snprintf(msg, sizeof(msg),
             "The display is reconnecting to “%s”. You can close this page.", esc);
    // Respond before triggering the save/reboot so the browser receives the page first.
    esp_err_t rc = send_result_page(req, "Saved ✓", msg, true);

    if (s_on_save) {
        s_on_save(&cfg, s_user);
    }
    return rc;
}

// Send a 302 to the portal root. Shared by the explicit OS-probe handlers and the catch-all
// 404 handler. Connection: close — iOS fires probes in rapid parallel bursts and idle
// keep-alive sockets would crowd out the real page requests.
static esp_err_t redirect_to_portal(httpd_req_t *req)
{
    no_keepalive(req);
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, "redirect", HTTPD_RESP_USE_STRLEN);
}

// Explicit handlers for the well-known OS captive-detection probe URLs. Registering them
// (instead of letting them fall through to the 404 handler) keeps the log clean: httpd logs
// a "URI not found" warning for every unregistered URI before invoking the error handler.
static esp_err_t probe_get(httpd_req_t *req)
{
    ESP_LOGI(TAG, "captive probe %s -> /", req->uri);
    return redirect_to_portal(req);
}

// Catch-all for any captive-detection URL not registered above (still redirected to setup).
static esp_err_t captive_404(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    ESP_LOGI(TAG, "captive redirect (404) %s -> /", req->uri);
    return redirect_to_portal(req);
}

static void start_http(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 16;  // "/" + "/save" + the OS captive-probe URLs below
    config.lru_purge_enable = true;
    // The captive WebView issues requests slowly and in bursts; the 5s default tears half-read
    // sockets down with recv errno 104. xiaozhi's esp-wifi-connect uses 15s for the same reason.
    config.recv_wait_timeout = 15;
    config.send_wait_timeout = 15;
    config.stack_size = 8192;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return;
    }

    const httpd_uri_t routes[] = {
        {.uri = "/",     .method = HTTP_GET,  .handler = index_get},
        {.uri = "/save", .method = HTTP_POST, .handler = save_post},
        // OS captive-detection probes — redirect to the portal. Registered explicitly so they
        // don't spam the log with "URI not found"; anything else still hits captive_404.
        {.uri = "/hotspot-detect.html",       .method = HTTP_GET, .handler = probe_get},  // iOS/macOS
        {.uri = "/library/test/success.html", .method = HTTP_GET, .handler = probe_get},  // iOS/macOS
        {.uri = "/success.html",              .method = HTTP_GET, .handler = probe_get},  // iOS
        {.uri = "/generate_204",              .method = HTTP_GET, .handler = probe_get},  // Android
        {.uri = "/gen_204",                   .method = HTTP_GET, .handler = probe_get},  // Android
        {.uri = "/ncsi.txt",                  .method = HTTP_GET, .handler = probe_get},  // Windows
        {.uri = "/connecttest.txt",           .method = HTTP_GET, .handler = probe_get},  // Windows
        {.uri = "/redirect",                  .method = HTTP_GET, .handler = probe_get},  // Windows
        {.uri = "/canonical.html",            .method = HTTP_GET, .handler = probe_get},  // Firefox
    };
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        httpd_register_uri_handler(server, &routes[i]);
    }
    httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, captive_404);
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
