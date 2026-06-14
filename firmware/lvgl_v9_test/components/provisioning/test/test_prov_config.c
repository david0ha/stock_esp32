#include "tf.h"
#include "prov_config.h"

TEST(ticker_normalize_uppercases_and_trims)
{
    char out[PROV_TICKER_MAX_LEN + 1];
    CHECK(prov_ticker_normalize("  aapl ", out) == true);
    CHECK_STR(out, "AAPL");
}

TEST(ticker_normalize_rejects_disallowed_chars)
{
    char out[PROV_TICKER_MAX_LEN + 1];
    CHECK(prov_ticker_normalize("ts$la", out) == false);
    CHECK_STR(out, "");
    CHECK(prov_ticker_normalize("aa pl", out) == false);  // internal space
    CHECK_STR(out, "");
}

TEST(ticker_normalize_rejects_too_long)
{
    char out[PROV_TICKER_MAX_LEN + 1];
    // 13 chars > PROV_TICKER_MAX_LEN (12)
    CHECK(prov_ticker_normalize("ABCDEFGHIJKLM", out) == false);
    CHECK_STR(out, "");
    // exactly 12 chars is accepted
    CHECK(prov_ticker_normalize("ABCDEFGHIJKL", out) == true);
    CHECK_STR(out, "ABCDEFGHIJKL");
}

TEST(ticker_normalize_rejects_empty)
{
    char out[PROV_TICKER_MAX_LEN + 1];
    CHECK(prov_ticker_normalize("", out) == false);
    CHECK_STR(out, "");
    CHECK(prov_ticker_normalize("   ", out) == false);
    CHECK_STR(out, "");
}
