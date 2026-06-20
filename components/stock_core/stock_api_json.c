#include "stock_api_json.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char HEX[] = "0123456789abcdef";

// Reset to an empty string and report failure. Centralizes the overflow contract.
static int fail(char *out, size_t out_size)
{
    if (out_size > 0) {
        out[0] = '\0';
    }
    return -1;
}

// Escape `in` as the contents of a JSON string (no surrounding quotes). Mirrors
// prov_json_escape, duplicated here so stock_core stays free of the provisioning dependency.
static int json_escape(const char *in, char *out, size_t out_size)
{
    if (out_size == 0) {
        return -1;
    }
    if (in == NULL) {
        in = "";
    }
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0'; i++) {
        unsigned char c = (unsigned char)in[i];
        const char *rep = NULL;
        char ubuf[7];
        size_t rlen = 1;
        switch (c) {
            case '"':  rep = "\\\""; rlen = 2; break;
            case '\\': rep = "\\\\"; rlen = 2; break;
            case '\b': rep = "\\b";  rlen = 2; break;
            case '\f': rep = "\\f";  rlen = 2; break;
            case '\n': rep = "\\n";  rlen = 2; break;
            case '\r': rep = "\\r";  rlen = 2; break;
            case '\t': rep = "\\t";  rlen = 2; break;
            default:
                if (c < 0x20) {
                    ubuf[0] = '\\'; ubuf[1] = 'u'; ubuf[2] = '0'; ubuf[3] = '0';
                    ubuf[4] = HEX[(c >> 4) & 0xF];
                    ubuf[5] = HEX[c & 0xF];
                    ubuf[6] = '\0';
                    rep = ubuf;
                    rlen = 6;
                }
                break;
        }
        if (rep != NULL) {
            if (o + rlen + 1 > out_size) {
                return fail(out, out_size);
            }
            memcpy(out + o, rep, rlen);
            o += rlen;
        } else {
            if (o + 1 + 1 > out_size) {
                return fail(out, out_size);
            }
            out[o++] = (char)c;
        }
    }
    out[o] = '\0';
    return (int)o;
}

// Format a finite double into `buf` with `prec` decimals. Non-finite values (NaN/inf, which a
// dead endpoint could leave behind) become "0" so the output is always valid, parseable JSON.
// A finite but huge magnitude can also be a hazard: "%.*f" of e.g. 1e20 overflows the small
// fixed buffer and truncates — possibly right on the decimal point ("10000000000000.") which
// strict JSON parsers (JS JSON.parse / Python json.loads) reject even though the lenient
// vendored cJSON accepts it. Detect the snprintf overflow and fall back to "0" so a garbage
// quote can never make the whole /api/stock/state document unparseable on the phone.
static void numstr(double v, int prec, char *buf, size_t n)
{
    if (!isfinite(v)) {
        snprintf(buf, n, "0");
        return;
    }
    int r = snprintf(buf, n, "%.*f", prec, v);
    if (r < 0 || (size_t)r >= n) {
        snprintf(buf, n, "0");
    }
}

int stock_api_json_info(char *out, size_t out_size,
                        const char *device_id, const char *model,
                        const char *fw, const char *ip)
{
    char e_id[STOCK_API_DEVID_MAXLEN * 6 + 1];
    char e_model[STOCK_API_MODEL_MAXLEN * 6 + 1];
    char e_fw[STOCK_API_FW_MAXLEN * 6 + 1];
    char e_ip[STOCK_API_IP_MAXLEN * 6 + 1];
    if (json_escape(device_id, e_id, sizeof(e_id)) < 0 ||
        json_escape(model, e_model, sizeof(e_model)) < 0 ||
        json_escape(fw, e_fw, sizeof(e_fw)) < 0 ||
        json_escape(ip, e_ip, sizeof(e_ip)) < 0) {
        return fail(out, out_size);
    }
    int n = snprintf(out, out_size,
                     "{\"deviceId\":\"%s\",\"model\":\"%s\",\"fw\":\"%s\",\"ip\":\"%s\"}",
                     e_id, e_model, e_fw, e_ip);
    if (n < 0 || (size_t)n >= out_size) {
        return fail(out, out_size);
    }
    return n;
}

int stock_api_json_state(const stock_api_state_t *st, char *out, size_t out_size)
{
    if (st == NULL || out_size == 0) {
        return fail(out, out_size);
    }

    char e_model[STOCK_API_MODEL_MAXLEN * 6 + 1];
    char e_fw[STOCK_API_FW_MAXLEN * 6 + 1];
    char e_id[STOCK_API_DEVID_MAXLEN * 6 + 1];
    char e_ip[STOCK_API_IP_MAXLEN * 6 + 1];
    if (json_escape(st->model, e_model, sizeof(e_model)) < 0 ||
        json_escape(st->fw, e_fw, sizeof(e_fw)) < 0 ||
        json_escape(st->device_id, e_id, sizeof(e_id)) < 0 ||
        json_escape(st->ip, e_ip, sizeof(e_ip)) < 0) {
        return fail(out, out_size);
    }

    char temp[16], hum[16], bv[16];
    numstr(st->env.temp_c, 1, temp, sizeof(temp));
    numstr(st->env.humidity, 1, hum, sizeof(hum));
    numstr(st->env.battery_v, 2, bv, sizeof(bv));

    // Envelope up to (and including) the opening of the watchlist array. A single snprintf so a
    // tight buffer fails cleanly before any tickers are appended.
    int head = snprintf(out, out_size,
        "{\"model\":\"%s\",\"fw\":\"%s\",\"deviceId\":\"%s\",\"ip\":\"%s\","
        "\"index\":%d,\"page\":%d,\"econMode\":%s,\"econWeek\":%d,\"refreshSeconds\":%d,"
        "\"keys\":{\"finnhub\":%s,\"fmp\":%s,\"econUrl\":%s},"
        "\"env\":{\"valid\":%s,\"tempC\":%s,\"humidity\":%s,"
        "\"batteryValid\":%s,\"batteryV\":%s,\"batteryPct\":%d},"
        "\"watchlist\":[",
        e_model, e_fw, e_id, e_ip,
        st->index, st->page, st->econ_mode ? "true" : "false", st->econ_week,
        st->refresh_seconds,
        st->keys.finnhub ? "true" : "false", st->keys.fmp ? "true" : "false",
        st->keys.econ_url ? "true" : "false",
        st->env.valid ? "true" : "false", temp, hum,
        st->env.battery_valid ? "true" : "false", bv, st->env.battery_pct);
    if (head < 0 || (size_t)head >= out_size) {
        return fail(out, out_size);
    }

    static const char CLOSE[] = "]}";
    const size_t close_len = sizeof(CLOSE) - 1;
    size_t o = (size_t)head;
    if (o + close_len + 1 > out_size) {
        return fail(out, out_size);   // no room even for an empty array
    }

    size_t count = st->ticker_count;
    if (count > STOCK_API_MAX_TICKERS) {
        count = STOCK_API_MAX_TICKERS;
    }
    bool first = true;
    for (size_t i = 0; i < count; i++) {
        const stock_api_ticker_t *t = &st->tickers[i];
        char e_sym[STOCK_API_SYMBOL_MAXLEN * 6 + 1];
        if (json_escape(t->symbol, e_sym, sizeof(e_sym)) < 0) {
            continue;
        }
        char price[24], change[24], percent[16];
        numstr(t->price, 4, price, sizeof(price));
        numstr(t->change, 4, change, sizeof(change));
        numstr(t->percent, 2, percent, sizeof(percent));

        char entry[160];
        int m = snprintf(entry, sizeof(entry),
            "%s{\"symbol\":\"%s\",\"valid\":%s,\"price\":%s,\"change\":%s,"
            "\"percent\":%s,\"ageSec\":%d}",
            first ? "" : ",", e_sym, t->valid ? "true" : "false",
            price, change, percent, t->age_sec);
        if (m < 0 || (size_t)m >= sizeof(entry)) {
            continue;
        }
        if (o + (size_t)m + close_len + 1 > out_size) {
            break;  // no room for this entry — stop; the array stays valid JSON
        }
        memcpy(out + o, entry, (size_t)m);
        o += (size_t)m;
        first = false;
    }

    memcpy(out + o, CLOSE, close_len);
    o += close_len;
    out[o] = '\0';
    return (int)o;
}
