// Pure (host-testable) helper for emitting safe JSON. No ESP-IDF dependency.
// Used by the captive-portal /scan and /state endpoints to escape network SSIDs and
// other text that may contain quotes, backslashes, or control characters.
#pragma once

#include <stddef.h>

// Size of an output buffer guaranteed to hold the escaped form of a `src_cap`-byte string
// (worst case: every byte becomes a 6-char \u00XX escape), including the trailing NUL.
#define PROV_JSON_ESCAPE_BUFSZ(src_cap) ((src_cap) * 6 + 1)

#ifdef __cplusplus
extern "C" {
#endif

// Escape `in` as a JSON string *body* (without the surrounding quotes) into `out`.
// Escapes '"' and '\\', maps \b \t \n \f \r to their short forms, and emits any other
// control byte (< 0x20) as \u00XX. Bytes >= 0x80 are passed through unchanged (UTF-8).
// `out` is always NUL-terminated. Returns the number of bytes written (excluding the NUL),
// or -1 if the escaped result would not fit in `out_size`.
int prov_json_escape(const char *in, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
