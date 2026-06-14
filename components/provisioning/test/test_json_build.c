#include "tf.h"
#include "json_build.h"

TEST(json_escape_passes_plain_text)
{
    char out[32];
    int n = prov_json_escape("AAPL", out, sizeof(out));
    CHECK_STR(out, "AAPL");
    CHECK_INT(n, 4);
}

TEST(json_escape_quotes_and_backslash)
{
    char out[32];
    prov_json_escape("a\"b", out, sizeof(out));
    CHECK_STR(out, "a\\\"b");   // a \" b
    prov_json_escape("a\\b", out, sizeof(out));
    CHECK_STR(out, "a\\\\b");   // a \\ b
}

TEST(json_escape_short_control_escapes)
{
    char out[32];
    prov_json_escape("a\nb\tc", out, sizeof(out));
    CHECK_STR(out, "a\\nb\\tc"); // a \n b \t c
}

TEST(json_escape_other_control_as_u_escape)
{
    char out[32];
    prov_json_escape("x\x01y", out, sizeof(out));
    CHECK_STR(out, "x\\u0001y");
}

TEST(json_escape_passes_high_bytes)
{
    char out[32];
    // UTF-8 bytes (>= 0x80) pass through untouched
    prov_json_escape("caf\xc3\xa9", out, sizeof(out));
    CHECK_STR(out, "caf\xc3\xa9");
}

TEST(json_escape_overflow_returns_negative)
{
    char out[4];
    CHECK_INT(prov_json_escape("a\"bc", out, sizeof(out)), -1);
    CHECK_STR(out, "");
}
