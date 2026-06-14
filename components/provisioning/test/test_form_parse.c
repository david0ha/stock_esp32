#include "tf.h"
#include "form_parse.h"

#include <string.h>

TEST(url_decode_plus_becomes_space)
{
    char out[32];
    int n = prov_url_decode("hello+world", out, sizeof(out));
    CHECK_STR(out, "hello world");
    CHECK_INT(n, 11);
}

TEST(url_decode_percent_hex)
{
    char out[32];
    CHECK_INT(prov_url_decode("AAPL%2CTSLA", out, sizeof(out)), 9);
    CHECK_STR(out, "AAPL,TSLA");
    prov_url_decode("%41%42%43", out, sizeof(out));
    CHECK_STR(out, "ABC");
}

TEST(url_decode_malformed_percent_is_literal)
{
    char out[32];
    prov_url_decode("100%", out, sizeof(out));
    CHECK_STR(out, "100%");
    prov_url_decode("a%2", out, sizeof(out));
    CHECK_STR(out, "a%2");
    prov_url_decode("a%ZZb", out, sizeof(out));
    CHECK_STR(out, "a%ZZb");
}

TEST(url_decode_overflow_returns_negative)
{
    char out[4];
    CHECK_INT(prov_url_decode("toolong", out, sizeof(out)), -1);
    CHECK_STR(out, "");
}

TEST(url_decode_rejects_embedded_nul)
{
    // %00 would inject a NUL that silently truncates a credential/ticker; reject it.
    char out[32];
    CHECK_INT(prov_url_decode("ab%00cd", out, sizeof(out)), -1);
    CHECK_STR(out, "");
}

TEST(form_get_field_extracts_and_decodes)
{
    const char *body = "ssid=My+Net&password=p%40ss&tickers=AAPL%2CTSLA";
    char out[64];
    CHECK(prov_form_get_field(body, "ssid", out, sizeof(out)) == true);
    CHECK_STR(out, "My Net");
    CHECK(prov_form_get_field(body, "password", out, sizeof(out)) == true);
    CHECK_STR(out, "p@ss");
    CHECK(prov_form_get_field(body, "tickers", out, sizeof(out)) == true);
    CHECK_STR(out, "AAPL,TSLA");
}

TEST(form_get_field_missing_returns_false)
{
    char out[32];
    CHECK(prov_form_get_field("a=1&b=2", "c", out, sizeof(out)) == false);
    CHECK_STR(out, "");
}

TEST(form_get_field_matches_whole_key_not_prefix)
{
    // "ticker" must not match the "tickers" field, and vice versa
    const char *body = "ticker=ONE&tickers=TWO";
    char out[32];
    CHECK(prov_form_get_field(body, "tickers", out, sizeof(out)) == true);
    CHECK_STR(out, "TWO");
    CHECK(prov_form_get_field(body, "ticker", out, sizeof(out)) == true);
    CHECK_STR(out, "ONE");
}

TEST(form_get_field_empty_value)
{
    const char *body = "ssid=&password=x";
    char out[32];
    CHECK(prov_form_get_field(body, "ssid", out, sizeof(out)) == true);
    CHECK_STR(out, "");
}
