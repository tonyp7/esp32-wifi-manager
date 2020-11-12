cmake_minimum_required(VERSION 3.5)

project(ruuvi_esp32-wifi-manager-tests)

add_definitions(
        -DRUUVI_TESTS=1
)

include_directories(
        esp_simul
)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(posix_gcc_simulator)

set(RUUVI_ESP_WRAPPERS ${CMAKE_CURRENT_SOURCE_DIR}/ruuvi.esp_wrappers.c)
set(RUUVI_ESP_WRAPPERS_INC ${RUUVI_ESP_WRAPPERS}/include)

set(RUUVI_ESP_WRAPPERS_TESTS_COMMON ${RUUVI_ESP_WRAPPERS}/tests/common)
set(RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC ${RUUVI_ESP_WRAPPERS_TESTS_COMMON}/include)

include_directories(
        ${RUUVI_ESP_WRAPPERS_INC}
        ${RUUVI_ESP_WRAPPERS_TESTS_COMMON_INC}
)

include_directories(
        "posix_gcc_simulator/FreeRTOS_Posix"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/include/freertos"
        "posix_gcc_simulator/FreeRTOS_Posix/FreeRTOS_Kernel/portable/GCC/Posix"
)

add_subdirectory(${RUUVI_ESP_WRAPPERS} ruuvi.esp_wrappers.c)
add_subdirectory(${RUUVI_ESP_WRAPPERS_TESTS_COMMON} ruuvi.esp_wrappers_tests)

add_subdirectory(lwip)

include_directories(
        "lwip/lwip/src/include"
        "lwip/extra"
        "lwip/lwip/contrib/ports/freertos/include"
        "lwip/lwip/contrib/ports/unix/port/include"
)

#include_directories(
#        "$ENV{IDF_PATH}/components/log/include/"
#        "$ENV{IDF_PATH}/components/esp_common/include/"
#        "$ENV{IDF_PATH}/components/esp_event/include/"
#        "$ENV{IDF_PATH}/components/"
#        "${CMAKE_BINARY_DIR}/../../../config/"
#)

add_subdirectory(test_ap_ssid)
add_subdirectory(test_json)
add_subdirectory(test_json_network_info)
add_subdirectory(test_json_access_points)
add_subdirectory(test_sta_ip_unsafe)
add_subdirectory(test_sta_ip_safe)

add_test(NAME test_ap_ssid
        COMMAND ruuvi_esp32-wifi-manager-test-ap_ssid
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-ap_ssid>/gtestresults.xml
)

add_test(NAME test_json
        COMMAND ruuvi_esp32-wifi-manager-test-json
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-json>/gtestresults.xml
)

add_test(NAME test_json_network_info
        COMMAND ruuvi_esp32-wifi-manager-test-json_network_info
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-json_network_info>/gtestresults.xml
)

add_test(NAME test_json_access_points
        COMMAND ruuvi_esp32-wifi-manager-test-json_access_points
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-json_access_points>/gtestresults.xml
)

add_test(NAME test_sta_ip_unsafe
        COMMAND ruuvi_esp32-wifi-manager-test-sta_ip_unsafe
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-sta_ip_unsafe>/gtestresults.xml
)

add_test(NAME test_sta_ip_safe
        COMMAND ruuvi_esp32-wifi-manager-test-sta_ip_safe
        --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-sta_ip_safe>/gtestresults.xml
)
