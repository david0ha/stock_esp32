cmake_minimum_required(VERSION 3.16)
project(lvgl_sim C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 우리 lv_conf.h 를 LVGL이 찾도록 (#include "lv_conf.h")
add_compile_definitions(LV_CONF_INCLUDE_SIMPLE)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# 빌드 속도/의존성 축소
set(LV_CONF_BUILD_DISABLE_EXAMPLES 1 CACHE INTERNAL "")
set(LV_CONF_BUILD_DISABLE_DEMOS 1 CACHE INTERNAL "")
set(LV_CONF_BUILD_DISABLE_THORVG_INTERNAL 1 CACHE INTERNAL "")

# ESP-IDF 예제에서 LVGL 소스와 UI(GUI Guider 생성)를 재사용
set(EX ${CMAKE_CURRENT_SOURCE_DIR}/../lvgl_v9_test)
add_subdirectory(${EX}/managed_components/lvgl__lvgl ${CMAKE_BINARY_DIR}/lvgl)

file(GLOB UI_SRC
    ${EX}/components/ui_bsp/generated/*.c
    ${EX}/components/ui_bsp/generated/images/*.c
    ${EX}/components/ui_bsp/custom/*.c)
include_directories(
    ${EX}/components/ui_bsp/generated
    ${EX}/components/ui_bsp/custom)

add_executable(sim main_sim.c ${UI_SRC})
target_link_libraries(sim lvgl)
