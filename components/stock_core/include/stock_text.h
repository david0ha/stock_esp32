/*
 * stock_text.h — tiny shared text helpers for the parsers.
 *
 * Header-only (static) so the firmware, simulator and host tests all compile
 * byte-identical copies without extra build wiring. Used by stock_parse.c and
 * econ_parse.c so API free-text is folded to ASCII in exactly one place — the
 * built-in mono font has no curly quotes / dashes / accented letters, so any
 * raw UTF-8 would otherwise render as tofu boxes.
 */
#pragma once

#include <stddef.h>
#include <string.h>

/* Bounded copy + NUL-terminate (truncates to fit `cap`). */
static void copy_cstr(char *dst, size_t cap, const char *src) {
    if (src) {
        strncpy(dst, src, cap - 1);
        dst[cap - 1] = '\0';
    } else {
        dst[0] = '\0';
    }
}

static void append(char *dst, size_t cap, size_t *j, const char *s) {
    for (; *s && *j < cap - 1; s++) dst[(*j)++] = *s;
}

/* Fold a UTF-8 string down to ASCII: common typographic punctuation is mapped
 * to its ASCII equivalent; any other non-ASCII byte sequence is dropped. */
static void to_ascii(char *dst, size_t cap, const char *src) {
    size_t j = 0;
    for (size_t i = 0; src && src[i] && j < cap - 1; ) {
        unsigned char c = (unsigned char)src[i];
        if (c < 0x80) {                       /* plain ASCII */
            dst[j++] = (char)c; i++;
        } else if (c == 0xE2 && (unsigned char)src[i + 1] == 0x80
                             && src[i + 2] != '\0') {   /* complete 3-byte seq only */
            switch ((unsigned char)src[i + 2]) {   /* General Punctuation */
                case 0x98: case 0x99: append(dst, cap, &j, "'");   break;
                case 0x9C: case 0x9D: append(dst, cap, &j, "\"");  break;
                case 0x93: case 0x94: append(dst, cap, &j, "-");   break;
                case 0xA6:            append(dst, cap, &j, "...");  break;
                default: break;                /* drop other punctuation */
            }
            i += 3;
        } else {                              /* drop any other UTF-8 char */
            i++;
            while (((unsigned char)src[i] & 0xC0) == 0x80) i++;
        }
    }
    dst[j] = '\0';
}
