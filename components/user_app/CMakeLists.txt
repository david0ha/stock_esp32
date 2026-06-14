idf_component_register(
    SRCS "user_app.cpp"
    REQUIRES provisioning          # user_app.h exposes prov_config_t (public dep)
    PRIV_REQUIRES stock_core app_bsp
    INCLUDE_DIRS "./")
