cmake_minimum_required(VERSION 3.5)

project(ruuvi_esp32-wifi-manager-test-sta_ip_safe)
set(ProjectId ruuvi_esp32-wifi-manager-test-sta_ip_safe)

add_executable(
        ${ProjectId}
        test_sta_ip_safe.cpp
        ../../src/sta_ip.h
        ../../src/sta_ip_safe.c
        ../../src/sta_ip_safe.h
        ../../src/sta_ip_unsafe.c
        ../../src/sta_ip_unsafe.h
        ../esp_simul/stub_func.c
)

set_target_properties(${ProjectId} PROPERTIES
        C_STANDARD 11
        CXX_STANDARD 14
)

target_include_directories(${ProjectId} PUBLIC
        ${gtest_SOURCE_DIR}/include
        ${gtest_SOURCE_DIR}
        ../../src
        include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(${ProjectId} PUBLIC
        RUUVI_TESTS_STA_IP_SAFE=1
)

target_compile_options(${ProjectId} PUBLIC
        -g3
        -ggdb
        -fprofile-arcs
        -ftest-coverage
        --coverage
)

target_link_libraries(${ProjectId}
        lwip
        FreeRTOS_Posix
        gtest
        gtest_main
        gcov
        ruuvi_esp_wrappers
        ruuvi_esp_wrappers-common_test_funcs
        --coverage
)
