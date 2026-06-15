/*
 * stock_parse.c — JSON -> model parsers (cJSON).
 *
 * Pure functions: no network, no globals. Unit-tested on the host against
 * captured API fixtures (components/stock_core/test/host). The firmware and
 * the desktop simulator compile this exact file.
 */
#include "stock_parse.h"
#include "stock_text.h"   /* copy_cstr / to_ascii (shared with econ_parse.c) */
#include "cJSON.h"
#include <string.h>
#include <math.h>
#include <stdint.h>

/* ---- small helpers ------------------------------------------------------ */

static double get_num(const cJSON *obj, const char *key, double def) {
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    /* reject NaN/Inf (cJSON accepts e.g. 1e400 -> Inf) so they never reach the UI */
    return (cJSON_IsNumber(it) && isfinite(it->valuedouble)) ? it->valuedouble : def;
}

/* Min or max of a JSON number array, skipping null/NaN. Returns `def` if empty. */
static double arr_extreme(const cJSON *arr, bool want_max, double def) {
    double best = def;
    bool any = false;
    for (int i = 0, k = cJSON_GetArraySize(arr); i < k; i++) {
        const cJSON *e = cJSON_GetArrayItem(arr, i);
        if (!cJSON_IsNumber(e) || !isfinite(e->valuedouble)) continue;
        double v = e->valuedouble;
        if (!any || (want_max ? v > best : v < best)) { best = v; any = true; }
    }
    return best;
}

/* Safe double -> int64 (JSON numbers can be NaN/Inf/huge; the raw cast is UB). */
static int64_t to_i64(double d) {
    if (isnan(d))        return 0;
    if (d >=  9.2e18)    return INT64_MAX;
    if (d <= -9.2e18)    return INT64_MIN;
    return (int64_t)d;
}

static void copy_str(char *dst, size_t cap, const cJSON *item) {
    copy_cstr(dst, cap, cJSON_IsString(item) ? item->valuestring : NULL);
}

/* ---- Finnhub /quote ----------------------------------------------------- */

int stock_parse_quote(const char *json, const char *symbol, stock_quote_t *out) {
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    const cJSON *c = cJSON_GetObjectItemCaseSensitive(root, "c");
    if (!cJSON_IsNumber(c) || !isfinite(c->valuedouble)) {       /* no/garbage price => no quote */
        cJSON_Delete(root);
        return -1;
    }

    copy_cstr(out->symbol, sizeof(out->symbol), symbol);
    out->price      = c->valuedouble;
    out->change     = get_num(root, "d", 0);
    out->percent    = get_num(root, "dp", 0);
    out->high       = get_num(root, "h", 0);
    out->low        = get_num(root, "l", 0);
    out->open       = get_num(root, "o", 0);
    out->prev_close = get_num(root, "pc", 0);
    out->timestamp  = to_i64(get_num(root, "t", 0));
    out->valid      = true;

    cJSON_Delete(root);
    return 0;
}

/* ---- Yahoo v8 /chart ---------------------------------------------------- */

int stock_parse_series(const char *json, stock_series_t *out) {
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    int rc = -1;
    const cJSON *chart  = cJSON_GetObjectItemCaseSensitive(root, "chart");
    const cJSON *result = cJSON_GetObjectItemCaseSensitive(chart, "result");
    const cJSON *r0     = cJSON_IsArray(result) ? cJSON_GetArrayItem(result, 0) : NULL;
    if (!r0) goto done;  /* error response: result is null/missing */

    const cJSON *meta   = cJSON_GetObjectItemCaseSensitive(r0, "meta");
    copy_str(out->symbol,   sizeof(out->symbol),   cJSON_GetObjectItemCaseSensitive(meta, "symbol"));
    copy_str(out->currency, sizeof(out->currency), cJSON_GetObjectItemCaseSensitive(meta, "currency"));
    out->prev_close = get_num(meta, "chartPreviousClose", get_num(meta, "previousClose", 0));
    out->gmtoffset  = (int32_t)get_num(meta, "gmtoffset", 0);

    const cJSON *ts     = cJSON_GetObjectItemCaseSensitive(r0, "timestamp");
    const cJSON *quote0 = cJSON_GetArrayItem(
        cJSON_GetObjectItemCaseSensitive(
            cJSON_GetObjectItemCaseSensitive(r0, "indicators"), "quote"), 0);
    const cJSON *close  = cJSON_GetObjectItemCaseSensitive(quote0, "close");
    if (!cJSON_IsArray(ts) || !cJSON_IsArray(close)) goto done;

    int n_ts = cJSON_GetArraySize(ts);
    int n_cl = cJSON_GetArraySize(close);
    int n = n_ts < n_cl ? n_ts : n_cl;

    float last = 0.0f; bool have_last = false;
    double mn = 1e18, mx = -1e18;
    int cnt = 0, last_real = -1;
    int64_t t_last_real = 0;
    for (int i = 0; i < n && cnt < STOCK_CANDLE_MAX; i++) {
        const cJSON *ti = cJSON_GetArrayItem(ts, i);
        if (!cJSON_IsNumber(ti)) continue;      /* no timestamp -> can't place on axis */
        int64_t t = to_i64(ti->valuedouble);

        const cJSON *ci = cJSON_GetArrayItem(close, i);
        bool real = cJSON_IsNumber(ci) && isfinite(ci->valuedouble);
        float v;
        if (real) {
            v = (float)ci->valuedouble; last = v; have_last = true;
        } else if (have_last) {                 /* interior null gap: carry last price */
            v = last;
        } else {
            continue;                           /* leading null: no trade yet */
        }

        out->close[cnt] = v;
        if (cnt == 0) out->t_start = t;
        if (real) { last_real = cnt; t_last_real = t; }
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        cnt++;
    }
    if (last_real < 0) goto done;  /* no real trades */

    /* true intraday high/low from the high[]/low[] arrays (close range understates it),
     * falling back to the close extremes when those arrays are absent */
    const cJSON *high = cJSON_GetObjectItemCaseSensitive(quote0, "high");
    const cJSON *low  = cJSON_GetObjectItemCaseSensitive(quote0, "low");

    out->count    = last_real + 1; /* drop trailing gaps so the line ends at "now" */
    out->t_end    = t_last_real;
    out->day_min  = mn;            /* close extremes -> chart Y range (line fills height) */
    out->day_max  = mx;
    out->day_high = arr_extreme(high, true,  mx);
    out->day_low  = arr_extreme(low,  false, mn);
    out->valid    = true;
    rc = 0;

done:
    cJSON_Delete(root);
    return rc;
}

/* ---- Finnhub /stock/metric --------------------------------------------- */

int stock_parse_metrics(const char *json, const char *symbol, stock_metrics_t *out) {
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;

    const cJSON *m = cJSON_GetObjectItemCaseSensitive(root, "metric");
    if (!cJSON_IsObject(m)) { cJSON_Delete(root); return -1; }

    copy_cstr(out->symbol, sizeof(out->symbol), symbol);
    out->pe_ttm      = get_num(m, "peTTM", 0);
    out->eps_ttm     = get_num(m, "epsTTM", 0);
    out->market_cap  = get_num(m, "marketCapitalization", 0);
    out->week52_high = get_num(m, "52WeekHigh", 0);
    out->week52_low  = get_num(m, "52WeekLow", 0);
    out->div_yield   = get_num(m, "dividendYieldIndicatedAnnual", 0);
    out->beta        = get_num(m, "beta", 0);
    out->valid       = true;

    cJSON_Delete(root);
    return 0;
}

/* ---- Finnhub /company-news --------------------------------------------- */

int stock_parse_news(const char *json, stock_news_t *out) {
    memset(out, 0, sizeof(*out));
    cJSON *root = cJSON_Parse(json);
    if (!root) return -1;
    if (!cJSON_IsArray(root)) { cJSON_Delete(root); return -1; }

    int total = cJSON_GetArraySize(root);
    int cnt = 0;
    for (int i = 0; i < total && cnt < STOCK_NEWS_MAX; i++) {
        const cJSON *a = cJSON_GetArrayItem(root, i);
        const cJSON *h = cJSON_GetObjectItemCaseSensitive(a, "headline");
        if (!cJSON_IsString(h) || !h->valuestring || h->valuestring[0] == '\0')
            continue;  /* skip empty headlines */
        stock_news_item_t *it = &out->items[cnt];
        to_ascii(it->headline, sizeof(it->headline), h->valuestring);
        const cJSON *src = cJSON_GetObjectItemCaseSensitive(a, "source");
        to_ascii(it->source, sizeof(it->source),
                 cJSON_IsString(src) ? src->valuestring : "");
        it->datetime = to_i64(get_num(a, "datetime", 0));
        cnt++;
    }
    out->count = cnt;
    out->valid = true;

    cJSON_Delete(root);
    return 0;
}
