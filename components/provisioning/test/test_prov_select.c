#include "tf.h"
#include "prov_config.h"

// prov_config_ticker_at() picks the active symbol for a rotation tick, wrapping
// modulo the watchlist length so the app cycles through every saved ticker. It
// returns NULL for an empty watchlist so the caller can fall back to a default.

TEST(ticker_at_empty_watchlist_is_null)
{
    prov_config_t cfg;
    prov_tickers_parse(&cfg, "");
    CHECK(prov_config_ticker_at(&cfg, 0) == NULL);
    CHECK(prov_config_ticker_at(&cfg, 5) == NULL);
}

TEST(ticker_at_single_ticker_ignores_index)
{
    prov_config_t cfg;
    prov_tickers_parse(&cfg, "AAPL");
    CHECK_STR(prov_config_ticker_at(&cfg, 0), "AAPL");
    CHECK_STR(prov_config_ticker_at(&cfg, 1), "AAPL");
    CHECK_STR(prov_config_ticker_at(&cfg, 99), "AAPL");
}

TEST(ticker_at_rotates_and_wraps)
{
    prov_config_t cfg;
    prov_tickers_parse(&cfg, "AAPL,TSLA,MSFT");
    CHECK_STR(prov_config_ticker_at(&cfg, 0), "AAPL");
    CHECK_STR(prov_config_ticker_at(&cfg, 1), "TSLA");
    CHECK_STR(prov_config_ticker_at(&cfg, 2), "MSFT");
    CHECK_STR(prov_config_ticker_at(&cfg, 3), "AAPL");  // wraps
    CHECK_STR(prov_config_ticker_at(&cfg, 4), "TSLA");
}
