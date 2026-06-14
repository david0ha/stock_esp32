idf_component_register(
    SRCS "user_app.cpp"
    PRIV_REQUIRES stock_core app_bsp nvs_flash
    INCLUDE_DIRS "./")
