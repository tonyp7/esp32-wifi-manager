/**
 * @file test_access_points_list.cpp
 * @author TheSomeMan
 * @date 2020-11-14
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "access_points_list.h"
#include <cstdio>
#include <cstring>
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestAccessPointsList : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
    }

    void
    TearDown() override
    {
    }

public:
    TestAccessPointsList();

    ~TestAccessPointsList() override;
};

TestAccessPointsList::TestAccessPointsList() = default;

TestAccessPointsList::~TestAccessPointsList() = default;

bool
operator==(const wifi_ap_record_t &p1, const wifi_ap_record_t &p2)
{
    return 0 == memcmp(&p1, &p2, sizeof(p1));
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestAccessPointsList, test_ap_list_clear_wifi_ap_record) // NOLINT
{
    wifi_ap_record_t wifi_ap = {
        .rssi = 10,
    };
    memset(&wifi_ap, 0xAA, sizeof(wifi_ap));
    ap_list_clear_wifi_ap_record(&wifi_ap);
    const wifi_ap_record_t exp_wifi_ap = { 0 };
    ASSERT_EQ(exp_wifi_ap, wifi_ap);
}

TEST_F(TestAccessPointsList, test_ap_list_clear_identical_ap_ssid_and_authmode_differs) // NOLINT
{
    wifi_ap_record_t ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 10,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    wifi_ap_record_t ap2 = {
        .ssid     = { 'q', 'w', 'e', '\0' },
        .rssi     = 20,
        .authmode = WIFI_AUTH_WPA3_PSK,
    };
    ap_list_clear_identical_ap(&ap1, &ap2);

    const wifi_ap_record_t exp_ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 10,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    const wifi_ap_record_t exp_ap2 = {
        .ssid     = { 'q', 'w', 'e', '\0' },
        .rssi     = 20,
        .authmode = WIFI_AUTH_WPA3_PSK,
    };
    ASSERT_EQ(exp_ap1, ap1);
    ASSERT_EQ(exp_ap2, ap2);
}

TEST_F(TestAccessPointsList, test_ap_list_clear_identical_ap_ssid_equals) // NOLINT
{
    wifi_ap_record_t ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 10,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    wifi_ap_record_t ap2 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 20,
        .authmode = WIFI_AUTH_WPA3_PSK,
    };
    ap_list_clear_identical_ap(&ap1, &ap2);

    const wifi_ap_record_t exp_ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 20,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    const wifi_ap_record_t exp_ap2 = {};
    ASSERT_EQ(exp_ap1, ap1);
    ASSERT_EQ(exp_ap2, ap2);
}

TEST_F(TestAccessPointsList, test_ap_list_clear_identical_ap_ssid_and_rssi_differs) // NOLINT
{
    wifi_ap_record_t ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 10,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    wifi_ap_record_t ap2 = {
        .ssid     = { 'q', 'w', 'e', '\0' },
        .rssi     = 20,
        .authmode = WIFI_AUTH_WPA3_PSK,
    };
    ap_list_clear_identical_ap(&ap1, &ap2);

    const wifi_ap_record_t exp_ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 10,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    const wifi_ap_record_t exp_ap2 = {
        .ssid     = { 'q', 'w', 'e', '\0' },
        .rssi     = 20,
        .authmode = WIFI_AUTH_WPA3_PSK,
    };
    ASSERT_EQ(exp_ap1, ap1);
    ASSERT_EQ(exp_ap2, ap2);
}

TEST_F(TestAccessPointsList, test_ap_list_clear_identical_ap_authmode_equals) // NOLINT
{
    wifi_ap_record_t ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 10,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    wifi_ap_record_t ap2 = {
        .ssid     = { 'q', 'w', 'e', '\0' },
        .rssi     = 20,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    ap_list_clear_identical_ap(&ap1, &ap2);

    const wifi_ap_record_t exp_ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = 10,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    const wifi_ap_record_t exp_ap2 = {
        .ssid     = { 'q', 'w', 'e', '\0' },
        .rssi     = 20,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    ASSERT_EQ(exp_ap1, ap1);
    ASSERT_EQ(exp_ap2, ap2);
}

TEST_F(TestAccessPointsList, test_ap_list_clear_identical_ap_ssid_and_authmode_equals_second_rssi_less) // NOLINT
{
    wifi_ap_record_t ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = -40,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    wifi_ap_record_t ap2 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = -50,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    ap_list_clear_identical_ap(&ap1, &ap2);

    const wifi_ap_record_t exp_ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = -40,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    const wifi_ap_record_t exp_ap2 = { 0 };
    ASSERT_EQ(exp_ap1, ap1);
    ASSERT_EQ(exp_ap2, ap2);
}

TEST_F(TestAccessPointsList, test_ap_list_clear_identical_ap_ssid_and_authmode_equals_first_rssi_less) // NOLINT
{
    wifi_ap_record_t ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = -50,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    wifi_ap_record_t ap2 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = -40,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    ap_list_clear_identical_ap(&ap1, &ap2);

    const wifi_ap_record_t exp_ap1 = {
        .ssid     = { 'a', 'b', 'c', '\0' },
        .rssi     = -40,
        .authmode = WIFI_AUTH_WPA2_PSK,
    };
    const wifi_ap_record_t exp_ap2 = { 0 };
    ASSERT_EQ(exp_ap1, ap1);
    ASSERT_EQ(exp_ap2, ap2);
}

TEST_F(TestAccessPointsList, ap_list_clear_identical_aps) // NOLINT
{
    wifi_ap_record_t arr_of_aps[7] = {
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -40,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        {
            .ssid     = { 'q', 'w', 'e', '\0' },
            .rssi     = -41,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        { 0 },
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -42,
            .authmode = WIFI_AUTH_WPA3_PSK,
        },
        {
            .ssid     = { 'q', 'w', 'e', '\0' },
            .rssi     = -43,
            .authmode = WIFI_AUTH_WPA3_PSK,
        },
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -10,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        {
            .ssid     = { 'q', 'w', 'e', '\0' },
            .rssi     = -50,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ap_list_clear_identical_aps(&arr_of_aps[0], sizeof(arr_of_aps) / sizeof(arr_of_aps[0]));
    const wifi_ap_record_t exp_arr_of_aps[7] = {
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -10,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        {
            .ssid     = { 'q', 'w', 'e', '\0' },
            .rssi     = -41,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        { 0 },
        { 0 },
        { 0 },
        { 0 },
        { 0 },
    };
    ASSERT_EQ(arr_of_aps[0], exp_arr_of_aps[0]);
    ASSERT_EQ(arr_of_aps[1], exp_arr_of_aps[1]);
    ASSERT_EQ(arr_of_aps[2], exp_arr_of_aps[2]);
    ASSERT_EQ(arr_of_aps[3], exp_arr_of_aps[3]);
    ASSERT_EQ(arr_of_aps[4], exp_arr_of_aps[4]);
    ASSERT_EQ(arr_of_aps[5], exp_arr_of_aps[5]);
    ASSERT_EQ(arr_of_aps[6], exp_arr_of_aps[6]);
}

TEST_F(TestAccessPointsList, ap_list_clear_identical_aps_empty) // NOLINT
{
    ap_list_clear_identical_aps(nullptr, 0);
}

TEST_F(TestAccessPointsList, ap_list_find_first_free_slot_0) // NOLINT
{
    ASSERT_EQ(nullptr, ap_list_find_first_free_slot(nullptr, 0));
}

TEST_F(TestAccessPointsList, ap_list_find_first_free_slot_1_found) // NOLINT
{
    wifi_ap_record_t arr_of_aps[1] = {
        { 0 },
    };
    ASSERT_EQ(&arr_of_aps[0], ap_list_find_first_free_slot(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));
}

TEST_F(TestAccessPointsList, ap_list_find_first_free_slot_1_not_found) // NOLINT
{
    wifi_ap_record_t arr_of_aps[1] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(nullptr, ap_list_find_first_free_slot(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));
}

TEST_F(TestAccessPointsList, ap_list_find_first_free_slot_2_found_first) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        { 0 },
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(&arr_of_aps[0], ap_list_find_first_free_slot(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));
}

TEST_F(TestAccessPointsList, ap_list_find_first_free_slot_2_found_first_2) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        { 0 },
        { 0 },
    };
    ASSERT_EQ(&arr_of_aps[0], ap_list_find_first_free_slot(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));
}

TEST_F(TestAccessPointsList, ap_list_find_first_free_slot_2_found_second) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        { 0 },
    };
    ASSERT_EQ(&arr_of_aps[1], ap_list_find_first_free_slot(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));
}

TEST_F(TestAccessPointsList, ap_list_find_first_free_slot_2_not_found) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        {
            .ssid = { 'q', 'w', 'e', '\0' },
        },
    };
    ASSERT_EQ(nullptr, ap_list_find_first_free_slot(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));
}

TEST_F(TestAccessPointsList, ap_list_reorder_empty) // NOLINT
{
    ASSERT_EQ(0, ap_list_reorder(nullptr, 0));
}

TEST_F(TestAccessPointsList, ap_list_reorder_1_empty) // NOLINT
{
    wifi_ap_record_t arr_of_aps[1] = {
        { 0 },
    };
    ASSERT_EQ(0, ap_list_reorder(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));
}

TEST_F(TestAccessPointsList, ap_list_reorder_1_non_empty) // NOLINT
{
    wifi_ap_record_t arr_of_aps[1] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(1, ap_list_reorder(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));

    const wifi_ap_record_t exp_arr_of_aps[1] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(exp_arr_of_aps[0], arr_of_aps[0]);
}

TEST_F(TestAccessPointsList, ap_list_reorder_2_empty) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        { 0 },
        { 0 },
    };
    ASSERT_EQ(0, ap_list_reorder(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));
}

TEST_F(TestAccessPointsList, ap_list_reorder_2_second_empty) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        { 0 },
    };
    ASSERT_EQ(1, ap_list_reorder(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));

    const wifi_ap_record_t exp_arr_of_aps[1] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(exp_arr_of_aps[0], arr_of_aps[0]);
}

TEST_F(TestAccessPointsList, ap_list_reorder_2_first_empty) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        { 0 },
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(1, ap_list_reorder(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));

    const wifi_ap_record_t exp_arr_of_aps[1] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(exp_arr_of_aps[0], arr_of_aps[0]);
}

TEST_F(TestAccessPointsList, ap_list_reorder_2_both_non_empty_equal) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(2, ap_list_reorder(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));

    const wifi_ap_record_t exp_arr_of_aps[2] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
    };
    ASSERT_EQ(exp_arr_of_aps[0], arr_of_aps[0]);
}

TEST_F(TestAccessPointsList, ap_list_reorder_2_both_non_empty_not_equal) // NOLINT
{
    wifi_ap_record_t arr_of_aps[2] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        {
            .ssid = { 'q', 'w', 'e', '\0' },
        },
    };
    ASSERT_EQ(2, ap_list_reorder(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));

    const wifi_ap_record_t exp_arr_of_aps[2] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        {
            .ssid = { 'q', 'w', 'e', '\0' },
        },
    };
    ASSERT_EQ(exp_arr_of_aps[0], arr_of_aps[0]);
}

TEST_F(TestAccessPointsList, ap_list_reorder_complex) // NOLINT
{
    wifi_ap_record_t arr_of_aps[] = {
        { 0 },
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        { 0 },
        {
            .ssid = { 'q', 'w', 'e', '\0' },
        },
        { 0 },
    };
    ASSERT_EQ(2, ap_list_reorder(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));

    const wifi_ap_record_t exp_arr_of_aps[2] = {
        {
            .ssid = { 'a', 'b', 'c', '\0' },
        },
        {
            .ssid = { 'q', 'w', 'e', '\0' },
        },
    };
    ASSERT_EQ(exp_arr_of_aps[0], arr_of_aps[0]);
}

TEST_F(TestAccessPointsList, ap_list_filter_unique_empty) // NOLINT
{
    ASSERT_EQ(0, ap_list_filter_unique(nullptr, 0));
}

TEST_F(TestAccessPointsList, ap_list_filter_unique_complex) // NOLINT
{
    wifi_ap_record_t arr_of_aps[] = {
        {
            .ssid = { '\0' },
        },
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -10,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        {
            .ssid = { '\0' },
        },
        {
            .ssid     = { 'q', 'w', 'e', '\0' },
            .rssi     = -20,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        {
            .ssid = { '\0' },
        },
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -10,
            .authmode = WIFI_AUTH_WPA3_PSK,
        },
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -40,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        {
            .ssid     = { 'q', 'w', 'e', '\0' },
            .rssi     = -30,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -9,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ASSERT_EQ(2, ap_list_filter_unique(arr_of_aps, sizeof(arr_of_aps) / sizeof(arr_of_aps[0])));

    const wifi_ap_record_t exp_arr_of_aps[] = {
        {
            .ssid     = { 'a', 'b', 'c', '\0' },
            .rssi     = -9,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
        {
            .ssid     = { 'q', 'w', 'e', '\0' },
            .rssi     = -20,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ASSERT_EQ(exp_arr_of_aps[0], arr_of_aps[0]);
    ASSERT_EQ(exp_arr_of_aps[1], arr_of_aps[1]);
}
