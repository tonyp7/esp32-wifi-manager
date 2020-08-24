cmake_minimum_required(VERSION 3.5)

project(ruuvi_esp32-wifi-manager-tests)

add_definitions(
        -DRUUVI_TESTS=1
)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(test_str_buf)
add_subdirectory(test_json)
add_subdirectory(test_json_network_info)
add_subdirectory(test_json_access_points)

add_test(NAME test_str_buf
        COMMAND ruuvi_esp32-wifi-manager-test-str_buf
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-str_buf>/gtestresults.xml
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
