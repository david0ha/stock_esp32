cmake_minimum_required(VERSION 3.16)
project(lvgl_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Our lv_conf.h (via #include "lv_conf.h").
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# Trim the LVGL build.
set(LV_CONF_BUILD_DISABLE_EXAMPLES 1 CACHE INTERNAL "")
set(LV_CONF_BUILD_DISABLE_DEMOS 1 CACHE INTERNAL "")
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL 1 CACHE INTERNAL "")

set(EX    ${CMAKE_CURRENT_SOURCE_DIR}/../lvgl_v9_test)
set(CORE  ${EX}/components/stock_core)
set(CJSON ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/cJSON)

# Reuse the firmware's LVGL source (fetched into managed_components).
add_subdirectory(${EX}/managed_components/lvgl__lvgl ${CMAKE_BINARY_DIR}/lvgl)

# The shared stock core is compiled verbatim; only the HTTP port is host-specific.
add_executable(sim
    main_sim.c
    http_port_curl.c
    ${CORE}/stock_parse.c
    ${CORE}/stock_service.c
    ${CORE}/ui_stock.c
    ${CJSON}/cJSON.c)

target_include_directories(sim PRIVATE ${CORE}/include ${CJSON})
target_link_libraries(sim lvgl curl m)
