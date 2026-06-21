idf_component_register(
    SRCS "user_app.cpp"
    # Public deps: user_app.h exposes prov_config_t (provisioning); user_app_control.h exposes
    # stock_api_state_t (stock_core) to the companion-app HTTP server (components/stock_api).
    REQUIRES provisioning stock_core
    PRIV_REQUIRES app_bsp buttons esp_timer board_io
    INCLUDE_DIRS "./")
