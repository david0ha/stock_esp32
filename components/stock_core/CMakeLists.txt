# IDF component: portable stock core + device-only ports.
# (The desktop simulator does NOT use this file — it compiles the portable
#  sources directly and supplies http_port_curl.c instead of http_port_esp.c.)
#
# cJSON is vendored (third_party/cJSON) rather than depended on: ESP-IDF v6 moved
# cJSON out of core components, and vendoring keeps the firmware, simulator and
# host tests on byte-identical parser code.
set(CJSON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/cJSON)

idf_component_register(
    SRCS
        "stock_parse.c"
        "stock_service.c"
        "econ_parse.c"
        "econ_service.c"
        "ui_stock.c"
        "ui_econ.c"
        "ui_home.c"
        "ui_font_black_50.c"
        "ui_font_black_26.c"
        "http_port_esp.c"
        "${CJSON_DIR}/cJSON.c"
    INCLUDE_DIRS "include" "${CJSON_DIR}"
    REQUIRES
        lvgl__lvgl
        esp_http_client
        esp-tls
        mbedtls)
