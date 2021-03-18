/**
 * @file test_ap_ssid.cpp
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include <string>
#include "ap_ssid.h"
#include "wifi_manager_defs.h"

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestApSsid : public ::testing::Test
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
    TestApSsid();

    ~TestApSsid() override;
};

TestApSsid::TestApSsid()
    : Test()
{
}

TestApSsid::~TestApSsid() = default;

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestApSsid, test_1) // NOLINT
{
    char ap_ssid[MAX_SSID_SIZE] = { 0 };

    const mac_address_bin_t ap_mac = {
        {
            0x11,
            0x22,
            0x33,
            0x44,
            0x55,
            0x66,
        },
    };
    ap_ssid_generate(ap_ssid, sizeof(ap_ssid), "my_ssid1", &ap_mac);
    ASSERT_EQ(string("my_ssid1 5566"), string(ap_ssid));
}

TEST_F(TestApSsid, test_long_ssid) // NOLINT
{
    char ap_ssid[MAX_SSID_SIZE] = { 0 };

    const mac_address_bin_t ap_mac = {
        {
            0x11,
            0x22,
            0x33,
            0x44,
            0x55,
            0x66,
        },
    };
    ap_ssid_generate(ap_ssid, sizeof(ap_ssid), "123456789012345678901234567890", &ap_mac);
    ASSERT_EQ(string("12345678901234567890123456 5566"), string(ap_ssid));
}
