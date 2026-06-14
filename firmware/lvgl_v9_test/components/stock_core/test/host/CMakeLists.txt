cmake_minimum_required(VERSION 3.16)
project(stock_core_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Paths relative to this file.
set(CORE   ${CMAKE_CURRENT_SOURCE_DIR}/../..)        # components/stock_core
set(CJSON  ${CORE}/../../../third_party/cJSON)        # firmware/third_party/cJSON

add_executable(test_parse
    test_parse.c
    ${CORE}/stock_parse.c
    ${CJSON}/cJSON.c)

target_include_directories(test_parse PRIVATE
    ${CORE}/include
    ${CJSON})

# Fixtures resolved by absolute path so the test runs from any cwd.
target_compile_definitions(test_parse PRIVATE FIXDIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_options(test_parse PRIVATE -Wall -Wextra -O0 -g)
target_link_libraries(test_parse m)

# Opt-in sanitizer build:  cmake -S . -B build -DSANITIZE=ON
# (Catches memory-safety regressions like UTF-8 over-reads; some sandboxed
#  macOS setups hang under ASan, so it's off by default.)
option(SANITIZE "Build with Address/UB sanitizers" OFF)
if(SANITIZE)
    target_compile_options(test_parse PRIVATE -fsanitize=address,undefined)
    target_link_options(test_parse PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME parse COMMAND test_parse)
