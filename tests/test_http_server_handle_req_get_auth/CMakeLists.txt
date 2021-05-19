cmake_minimum_required(VERSION 3.7)

project(ruuvi_esp32-wifi-manager-test-http_server_handle_req_get_auth)
set(ProjectId ruuvi_esp32-wifi-manager-test-http_server_handle_req_get_auth)

add_executable(${ProjectId}
        test_http_server_handle_req_get_auth.cpp
        ../../src/http_server_auth.c
        ../../src/http_server_auth.h
        ../../src/http_server_auth_common.h
        ../../src/http_server_auth_digest.c
        ../../src/http_server_auth_digest.h
        ../../src/http_server_auth_ruuvi.c
        ../../src/http_server_auth_ruuvi.h
        ../../src/http_server_handle_req_delete_auth.c
        ../../src/http_server_handle_req_delete_auth.h
        ../../src/http_server_handle_req_get_auth.c
        ../../src/http_server_handle_req_get_auth.h
        ../../src/http_server_handle_req_post_auth.c
        ../../src/http_server_handle_req_post_auth.h
        ../../src/wifiman_md5.c
        ../../src/wifiman_md5.h
        ../../src/wifiman_sha256.c
        ../../src/wifiman_sha256.h
        ../../src/http_server_resp.c
        ../../src/include/http_server_resp.h
        ../../src/http_req.c
        ../../src/http_req.h
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.c
        $ENV{IDF_PATH}/components/json/cJSON/cJSON.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/base64.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/base64.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/md5.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/md5.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/sha256.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/sha256.h
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/library/platform_util.c
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include/mbedtls/platform_util.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        $ENV{IDF_PATH}/components/mbedtls/mbedtls/include
        $ENV{IDF_PATH}/components/json/cJSON
        ../../src/include
        ../../src
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_HTTP_SERVER_HANDLE_REQ_GET_AUTH=1
)

target_compile_options(${ProjectId} PUBLIC
        -g3
        -ggdb
        -fprofile-arcs
        -ftest-coverage
        --coverage
)

# CMake has a target_link_options starting from version 3.13
#target_link_options(${ProjectId} PUBLIC
#        --coverage
#)

target_link_libraries(${ProjectId}
        gtest
        gtest_main
        gcov
        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)
