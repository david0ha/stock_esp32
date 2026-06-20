cmake_minimum_required(VERSION 3.16)
project(stock_core_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Paths relative to this file.
set(CORE   ${CMAKE_CURRENT_SOURCE_DIR}/../..)        # components/stock_core
set(CJSON  ${CORE}/../../third_party/cJSON)           # <root>/third_party/cJSON

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

# stock_service tests: link a fake http_get (defined in test_service.c) so the
# service layer is exercised on the host with no network.
add_executable(test_service
    test_service.c
    ${CORE}/stock_service.c
    ${CORE}/stock_parse.c
    ${CJSON}/cJSON.c)

target_include_directories(test_service PRIVATE
    ${CORE}/include
    ${CJSON})

target_compile_options(test_service PRIVATE -Wall -Wextra -O0 -g)
target_link_libraries(test_service m)

if(SANITIZE)
    target_compile_options(test_service PRIVATE -fsanitize=address,undefined)
    target_link_options(test_service PRIVATE -fsanitize=address,undefined)
endif()

# economic-calendar parse/time helpers
add_executable(test_econ
    test_econ.c
    ${CORE}/econ_parse.c
    ${CJSON}/cJSON.c)

target_include_directories(test_econ PRIVATE
    ${CORE}/include
    ${CJSON})

target_compile_definitions(test_econ PRIVATE FIXDIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
target_compile_options(test_econ PRIVATE -Wall -Wextra -O0 -g)
target_link_libraries(test_econ m)

if(SANITIZE)
    target_compile_options(test_econ PRIVATE -fsanitize=address,undefined)
    target_link_options(test_econ PRIVATE -fsanitize=address,undefined)
endif()

# econ_service tests: link a fake http_get (defined in test_econ_service.c).
add_executable(test_econ_service
    test_econ_service.c
    ${CORE}/econ_service.c
    ${CORE}/econ_parse.c
    ${CJSON}/cJSON.c)

target_include_directories(test_econ_service PRIVATE
    ${CORE}/include
    ${CJSON})

target_compile_options(test_econ_service PRIVATE -Wall -Wextra -O0 -g)
target_link_libraries(test_econ_service m)

if(SANITIZE)
    target_compile_options(test_econ_service PRIVATE -fsanitize=address,undefined)
    target_link_options(test_econ_service PRIVATE -fsanitize=address,undefined)
endif()

# stock_api_json: pure serializers for the companion-app control API. Linked against cJSON only
# to validate the output is well-formed JSON with the expected fields.
add_executable(test_api_json
    test_api_json.c
    ${CORE}/stock_api_json.c
    ${CJSON}/cJSON.c)

target_include_directories(test_api_json PRIVATE
    ${CORE}/include
    ${CJSON})

target_compile_options(test_api_json PRIVATE -Wall -Wextra -O0 -g)
target_link_libraries(test_api_json m)

if(SANITIZE)
    target_compile_options(test_api_json PRIVATE -fsanitize=address,undefined)
    target_link_options(test_api_json PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME parse COMMAND test_parse)
add_test(NAME service COMMAND test_service)
add_test(NAME econ COMMAND test_econ)
add_test(NAME econ_service COMMAND test_econ_service)
add_test(NAME api_json COMMAND test_api_json)
