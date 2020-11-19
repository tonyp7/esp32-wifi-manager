cmake_minimum_required(VERSION 3.7)

project(ruuvi_esp32-wifi-manager-test-json)
set(ProjectId ruuvi_esp32-wifi-manager-test-json)

add_executable(${ProjectId}
        test_json.cpp
        ../../src/json.c
        ../../src/json.h
        ../../src/json_network_info.c
        ../../src/json_network_info.h
        ../../src/include/wifi_manager_defs.h
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ../../src/include
        ../../src
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_JSON=1
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
