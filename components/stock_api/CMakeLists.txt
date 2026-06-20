idf_component_register(
    SRCS "stock_api.c"
    INCLUDE_DIRS "."
    # stock_core: stock_api_json + the vendored cJSON include/symbols.
    # user_app:   the user_app_control bridge (snapshot + command posters).
    PRIV_REQUIRES
        stock_core
        user_app
        esp_http_server
        esp_netif
        esp_wifi
        esp_hw_support
        esp_event
        mdns)
