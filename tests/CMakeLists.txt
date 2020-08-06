cmake_minimum_required(VERSION 3.5)

project(ruuvi_esp32-wifi-manager-tests)

add_definitions(
        -DRUUVI_TESTS=1
)

enable_testing()
add_subdirectory(googletest)
add_subdirectory(test_stub)

add_test(NAME test_stub
        COMMAND ruuvi_esp32-wifi-manager-test-stub
            --gtest_output=xml:$<TARGET_FILE_DIR:ruuvi_esp32-wifi-manager-test-stub>/gtestresults.xml
)
