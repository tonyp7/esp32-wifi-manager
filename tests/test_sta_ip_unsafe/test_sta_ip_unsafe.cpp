/**
 * @file test_sta_ip_unsafe.cpp
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include <string>
#include "sta_ip_unsafe.h"

using namespace std;

/*** Google-test class implementation *********************************************************************************/

class TestStaIpUnsafe;

static TestStaIpUnsafe *g_pTestClass;

class TestStaIpUnsafe : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        g_pTestClass = this;
    }

    void
    TearDown() override
    {
        g_pTestClass = nullptr;
    }

public:
    TestStaIpUnsafe();

    ~TestStaIpUnsafe() override;
};

TestStaIpUnsafe::TestStaIpUnsafe()
    : Test()
{
}

TestStaIpUnsafe::~TestStaIpUnsafe() = default;

/*** Unit-Tests *******************************************************************************************************/

#ifdef __cplusplus
extern "C" {
#endif

void
tdd_assert_trap(void)
{
    printf("assert\n");
}

static volatile int32_t g_flagDisableCheckIsThreadFreeRTOS;

void
disableCheckingIfCurThreadIsFreeRTOS(void)
{
    ++g_flagDisableCheckIsThreadFreeRTOS;
}

void
enableCheckingIfCurThreadIsFreeRTOS(void)
{
    --g_flagDisableCheckIsThreadFreeRTOS;
    assert(g_flagDisableCheckIsThreadFreeRTOS >= 0);
}

int
checkIfCurThreadIsFreeRTOS(void)
{
    return false;
}

unsigned int
lwip_port_rand(void)
{
    return (unsigned int)random();
}

#ifdef __cplusplus
}
#endif

TEST_F(TestStaIpUnsafe, test_127_0_0_3) // NOLINT
{
    sta_ip_unsafe_init();
    ASSERT_EQ(string("0.0.0.0"), string(sta_ip_unsafe_get_copy().buf));

    const uint32_t ip_addr = sta_ip_unsafe_conv_str_to_ip("127.0.0.3");
    ASSERT_EQ(0x0300007f, ip_addr);
    sta_ip_unsafe_set(ip_addr);
    ASSERT_EQ(string("127.0.0.3"), string(sta_ip_unsafe_get_copy().buf));

    sta_ip_unsafe_reset();
    ASSERT_EQ(string("0.0.0.0"), string(sta_ip_unsafe_get_copy().buf));

    sta_ip_unsafe_deinit();
    ASSERT_EQ(string(""), string(sta_ip_unsafe_get_copy().buf));
}

TEST_F(TestStaIpUnsafe, test_192_168_1_10) // NOLINT
{
    sta_ip_unsafe_init();
    ASSERT_EQ(string("0.0.0.0"), string(sta_ip_unsafe_get_copy().buf));

    const uint32_t ip_addr = sta_ip_unsafe_conv_str_to_ip("192.168.1.10");
    ASSERT_EQ(0x0a01a8c0, ip_addr);
    sta_ip_unsafe_set(ip_addr);
    ASSERT_EQ(string("192.168.1.10"), string(sta_ip_unsafe_get_copy().buf));

    sta_ip_unsafe_reset();
    ASSERT_EQ(string("0.0.0.0"), string(sta_ip_unsafe_get_copy().buf));

    sta_ip_unsafe_deinit();
    ASSERT_EQ(string(""), string(sta_ip_unsafe_get_copy().buf));
}
