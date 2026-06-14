#include "tf.h"
#include "prov_config.h"

TEST(tickers_parse_splits_and_normalizes)
{
    prov_config_t cfg;
    size_t n = prov_tickers_parse(&cfg, "aapl, tsla  msft");
    CHECK_INT(n, 3);
    CHECK_INT(cfg.ticker_count, 3);
    CHECK_STR(cfg.tickers[0], "AAPL");
    CHECK_STR(cfg.tickers[1], "TSLA");
    CHECK_STR(cfg.tickers[2], "MSFT");
}

TEST(tickers_parse_dedupes_case_insensitively)
{
    prov_config_t cfg;
    size_t n = prov_tickers_parse(&cfg, "AAPL,aapl,AaPl,TSLA");
    CHECK_INT(n, 2);
    CHECK_STR(cfg.tickers[0], "AAPL");
    CHECK_STR(cfg.tickers[1], "TSLA");
}

TEST(tickers_parse_skips_invalid_tokens)
{
    prov_config_t cfg;
    size_t n = prov_tickers_parse(&cfg, "AAPL,$$$,,T@SLA,MSFT");
    CHECK_INT(n, 2);
    CHECK_STR(cfg.tickers[0], "AAPL");
    CHECK_STR(cfg.tickers[1], "MSFT");
}

TEST(tickers_parse_caps_at_max)
{
    prov_config_t cfg;
    // 20 distinct valid symbols, only PROV_MAX_TICKERS kept
    size_t n = prov_tickers_parse(
        &cfg, "A1,A2,A3,A4,A5,A6,A7,A8,A9,A10,A11,A12,A13,A14,A15,A16,A17,A18,A19,A20");
    CHECK_INT(n, PROV_MAX_TICKERS);
    CHECK_INT(cfg.ticker_count, PROV_MAX_TICKERS);
    CHECK_STR(cfg.tickers[0], "A1");
    CHECK_STR(cfg.tickers[PROV_MAX_TICKERS - 1], "A16");
}

TEST(tickers_parse_handles_empty_and_null)
{
    prov_config_t cfg;
    CHECK_INT(prov_tickers_parse(&cfg, ""), 0);
    CHECK_INT(cfg.ticker_count, 0);
    CHECK_INT(prov_tickers_parse(&cfg, "   , ,  "), 0);
    CHECK_INT(prov_tickers_parse(&cfg, NULL), 0);
}

TEST(tickers_serialize_joins_with_commas)
{
    prov_config_t cfg;
    prov_tickers_parse(&cfg, "AAPL TSLA MSFT");
    char out[64];
    size_t len = prov_tickers_serialize(&cfg, out, sizeof(out));
    CHECK_STR(out, "AAPL,TSLA,MSFT");
    CHECK_INT(len, 14);
}

TEST(tickers_serialize_round_trips_through_parse)
{
    prov_config_t a;
    prov_tickers_parse(&a, "nvda,amd,intc");
    char buf[64];
    prov_tickers_serialize(&a, buf, sizeof(buf));

    prov_config_t b;
    prov_tickers_parse(&b, buf);
    CHECK_INT(b.ticker_count, 3);
    CHECK_STR(b.tickers[0], "NVDA");
    CHECK_STR(b.tickers[1], "AMD");
    CHECK_STR(b.tickers[2], "INTC");
}

TEST(tickers_serialize_empty_is_empty_string)
{
    prov_config_t cfg;
    prov_tickers_parse(&cfg, "");
    char out[8];
    size_t len = prov_tickers_serialize(&cfg, out, sizeof(out));
    CHECK_INT(len, 0);
    CHECK_STR(out, "");
}
