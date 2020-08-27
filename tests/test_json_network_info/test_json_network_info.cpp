/**
 * @file test_json_network_info.cpp
 * @author TheSomeMan
 * @date 2020-08-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "json_network_info.h"
#include <string>

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestJsonNetworkInfo : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        json_network_info_init();
    }

    void
    TearDown() override
    {
        json_network_info_deinit();
    }

public:
    TestJsonNetworkInfo();

    ~TestJsonNetworkInfo() override;
};

TestJsonNetworkInfo::TestJsonNetworkInfo() = default;

TestJsonNetworkInfo::~TestJsonNetworkInfo() = default;

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestJsonNetworkInfo, test_after_init) // NOLINT
{
    const char *json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), string(json_str));
}

TEST_F(TestJsonNetworkInfo, test_clear) // NOLINT
{
    json_network_info_clear();
    const char *json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), string(json_str));
}

TEST_F(TestJsonNetworkInfo, test_generate_ssid_null) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };
    json_network_info_generate(nullptr, &network_info, UPDATE_CONNECTION_OK);
    const char *json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), string(json_str));
}

TEST_F(TestJsonNetworkInfo, test_generate_connection_ok) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };
    json_network_info_generate("test_ssid", &network_info, UPDATE_CONNECTION_OK);
    const char *json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"192.168.0.50\","
               "\"netmask\":\"255.255.255.0\","
               "\"gw\":\"192.168.0.1\","
               "\"urc\":0"
               "}\n"),
        string(json_str));
}

TEST_F(TestJsonNetworkInfo, test_generate_failed_attempt) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };
    json_network_info_generate("test_ssid", &network_info, UPDATE_FAILED_ATTEMPT);
    const char *json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"urc\":1"
               "}\n"),
        string(json_str));
}

TEST_F(TestJsonNetworkInfo, test_generate_user_disconnect) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };
    json_network_info_generate("test_ssid", &network_info, UPDATE_USER_DISCONNECT);
    const char *json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"urc\":2"
               "}\n"),
        string(json_str));
}

TEST_F(TestJsonNetworkInfo, test_generate_lost_connection) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };
    json_network_info_generate("test_ssid", &network_info, UPDATE_LOST_CONNECTION);
    const char *json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"urc\":3"
               "}\n"),
        string(json_str));
}
