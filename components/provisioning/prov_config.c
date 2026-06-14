#include "prov_config.h"

#include <string.h>

static bool is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static bool is_symbol_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-';
}

static bool is_separator(char c)
{
    return is_space(c) || c == ',';
}

bool prov_ticker_normalize(const char *raw, char *out)
{
    const char *end = raw + strlen(raw);
    while (*raw && is_space(*raw)) {
        raw++;
    }
    while (end > raw && is_space(end[-1])) {
        end--;
    }

    size_t n = 0;
    for (const char *p = raw; p < end; p++) {
        char c = *p;
        if (c >= 'a' && c <= 'z') {
            c = (char)(c - 'a' + 'A');
        }
        if (!is_symbol_char(c) || n >= PROV_TICKER_MAX_LEN) {
            out[0] = '\0';
            return false;
        }
        out[n++] = c;
    }
    out[n] = '\0';
    return n > 0;
}

size_t prov_tickers_parse(prov_config_t *cfg, const char *input)
{
    cfg->ticker_count = 0;
    if (input == NULL) {
        return 0;
    }

    const char *p = input;
    while (*p && cfg->ticker_count < PROV_MAX_TICKERS) {
        while (*p && is_separator(*p)) {
            p++;
        }
        const char *start = p;
        while (*p && !is_separator(*p)) {
            p++;
        }
        size_t tok_len = (size_t)(p - start);
        if (tok_len == 0) {
            continue;  // trailing separators
        }

        char token[64];
        if (tok_len >= sizeof(token)) {
            continue;  // absurdly long token: not a valid symbol, skip
        }
        memcpy(token, start, tok_len);
        token[tok_len] = '\0';

        char norm[PROV_TICKER_MAX_LEN + 1];
        if (!prov_ticker_normalize(token, norm)) {
            continue;
        }

        bool duplicate = false;
        for (size_t i = 0; i < cfg->ticker_count; i++) {
            if (strcmp(cfg->tickers[i], norm) == 0) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            // norm is already bounded by prov_ticker_normalize; strlcpy makes that explicit.
            strlcpy(cfg->tickers[cfg->ticker_count], norm,
                    sizeof(cfg->tickers[cfg->ticker_count]));
            cfg->ticker_count++;
        }
    }
    return cfg->ticker_count;
}

size_t prov_tickers_serialize(const prov_config_t *cfg, char *out, size_t out_size)
{
    if (out_size == 0) {
        return 0;
    }
    size_t len = 0;
    out[0] = '\0';
    for (size_t i = 0; i < cfg->ticker_count; i++) {
        const char *sym = cfg->tickers[i];
        size_t sym_len = strlen(sym);
        size_t need = sym_len + (i > 0 ? 1u : 0u);
        if (len + need + 1 > out_size) {
            break;  // would not fit (with NUL): stop at a clean comma boundary
        }
        if (i > 0) {
            out[len++] = ',';
        }
        memcpy(out + len, sym, sym_len);
        len += sym_len;
        out[len] = '\0';
    }
    return len;
}

const char *prov_config_ticker_at(const prov_config_t *cfg, size_t index)
{
    if (cfg->ticker_count == 0) {
        return NULL;
    }
    return cfg->tickers[index % cfg->ticker_count];
}
