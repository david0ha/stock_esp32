#include "json_build.h"

#include <string.h>

int prov_json_escape(const char *in, char *out, size_t out_size)
{
    if (out_size == 0) {
        return -1;
    }
    static const char hex[] = "0123456789abcdef";
    size_t oi = 0;
    for (const unsigned char *p = (const unsigned char *)in; *p; p++) {
        unsigned char c = *p;
        char esc[6];
        size_t esc_len;
        switch (c) {
            case '"':  esc[0] = '\\'; esc[1] = '"';  esc_len = 2; break;
            case '\\': esc[0] = '\\'; esc[1] = '\\'; esc_len = 2; break;
            case '\b': esc[0] = '\\'; esc[1] = 'b';  esc_len = 2; break;
            case '\f': esc[0] = '\\'; esc[1] = 'f';  esc_len = 2; break;
            case '\n': esc[0] = '\\'; esc[1] = 'n';  esc_len = 2; break;
            case '\r': esc[0] = '\\'; esc[1] = 'r';  esc_len = 2; break;
            case '\t': esc[0] = '\\'; esc[1] = 't';  esc_len = 2; break;
            default:
                if (c < 0x20) {
                    esc[0] = '\\'; esc[1] = 'u'; esc[2] = '0'; esc[3] = '0';
                    esc[4] = hex[(c >> 4) & 0xF];
                    esc[5] = hex[c & 0xF];
                    esc_len = 6;
                } else {
                    esc[0] = (char)c;
                    esc_len = 1;
                }
                break;
        }
        if (oi + esc_len + 1 > out_size) {
            out[0] = '\0';
            return -1;
        }
        memcpy(out + oi, esc, esc_len);
        oi += esc_len;
    }
    out[oi] = '\0';
    return (int)oi;
}
