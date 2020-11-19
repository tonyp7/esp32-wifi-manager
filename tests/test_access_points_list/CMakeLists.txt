cmake_minimum_required(VERSION 3.7)

project(ruuvi_esp32-wifi-manager-test-access_points_list)
set(ProjectId ruuvi_esp32-wifi-manager-test-access_points_list)

add_executable(${ProjectId}
        test_access_points_list.cpp
        include/esp_wifi_types.h
        include/esp_event_base.h
        ../../src/access_points_list.c
        ../../src/access_points_list.h
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
        RUUVI_TESTS_ACCESS_POINTS_LIST=1
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
