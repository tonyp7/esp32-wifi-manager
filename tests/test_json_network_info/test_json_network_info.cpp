/**
 * @file test_json_network_info.cpp
 * @author TheSomeMan
 * @date 2020-08-23
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "gtest/gtest.h"
#include "json_network_info.h"
#include <string>
#include "os_mutex.h"

using namespace std;

class TestJsonNetworkInfo;
static TestJsonNetworkInfo *g_pTestClass;

class MemAllocTrace
{
    vector<void *> allocated_mem;

    std::vector<void *>::iterator
    find(void *ptr)
    {
        for (auto iter = this->allocated_mem.begin(); iter != this->allocated_mem.end(); ++iter)
        {
            if (*iter == ptr)
            {
                return iter;
            }
        }
        return this->allocated_mem.end();
    }

public:
    void
    add(void *ptr)
    {
        auto iter = find(ptr);
        assert(iter == this->allocated_mem.end()); // ptr was found in the list of allocated memory blocks
        this->allocated_mem.push_back(ptr);
    }
    void
    remove(void *ptr)
    {
        auto iter = find(ptr);
        assert(iter != this->allocated_mem.end()); // ptr was not found in the list of allocated memory blocks
        this->allocated_mem.erase(iter);
    }
    bool
    is_empty()
    {
        return this->allocated_mem.empty();
    }
};

/*** Google-test class implementation *********************************************************************************/

class TestJsonNetworkInfo : public ::testing::Test
{
private:
protected:
    void
    SetUp() override
    {
        json_network_info_init();
        g_pTestClass = this;

        this->m_malloc_cnt         = 0;
        this->m_malloc_fail_on_cnt = 0;
    }

    void
    TearDown() override
    {
        json_network_info_deinit();
        g_pTestClass = nullptr;
    }

public:
    TestJsonNetworkInfo();

    ~TestJsonNetworkInfo() override;

    MemAllocTrace m_mem_alloc_trace {};
    uint32_t      m_malloc_cnt {};
    uint32_t      m_malloc_fail_on_cnt {};
};

TestJsonNetworkInfo::TestJsonNetworkInfo() = default;

TestJsonNetworkInfo::~TestJsonNetworkInfo() = default;

extern "C" {

void *
os_malloc(const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *ptr = malloc(size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

void
os_free_internal(void *ptr)
{
    g_pTestClass->m_mem_alloc_trace.remove(ptr);
    free(ptr);
}

void *
os_calloc(const size_t nmemb, const size_t size)
{
    if (++g_pTestClass->m_malloc_cnt == g_pTestClass->m_malloc_fail_on_cnt)
    {
        return nullptr;
    }
    void *ptr = calloc(nmemb, size);
    assert(nullptr != ptr);
    g_pTestClass->m_mem_alloc_trace.add(ptr);
    return ptr;
}

os_mutex_t
os_mutex_create_static(os_mutex_static_t *const p_mutex_static)
{
    return reinterpret_cast<os_mutex_t>(p_mutex_static);
}

void
os_mutex_delete(os_mutex_t *p_mutex)
{
    if (nullptr != *p_mutex)
    {
        *p_mutex = nullptr;
    }
}

bool
os_mutex_lock_with_timeout(os_mutex_t const h_mutex, const os_delta_ticks_t ticks_to_wait)
{
    return true;
}

void
os_mutex_unlock(os_mutex_t const h_mutex)
{
}

} // extern "C"

static void
json_network_info_do_get(json_network_info_t *const p_info, void *const p_param)
{
    auto p_json_info_copy = static_cast<string *>(p_param);
    p_json_info_copy->append(p_info->json_buf);
}

string
json_network_info_get()
{
    string json_info_copy {};
    json_network_info_do_action(&json_network_info_do_get, &json_info_copy);
    return json_info_copy;
}

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestJsonNetworkInfo, test_after_init) // NOLINT
{
    string json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), json_str);
}

TEST_F(TestJsonNetworkInfo, test_clear) // NOLINT
{
    json_network_info_clear();
    string json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_ssid_null) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };
    json_network_info_generate(nullptr, &network_info, UPDATE_CONNECTION_OK);
    string json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_ssid_empty) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };
    const wifi_ssid_t ssid = { "" };
    json_network_info_generate(&ssid, &network_info, UPDATE_CONNECTION_OK);
    string json_str = json_network_info_get();
    ASSERT_EQ(string("{}\n"), json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_connection_ok) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };

    const wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_generate(&ssid, &network_info, UPDATE_CONNECTION_OK);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"192.168.0.50\","
               "\"netmask\":\"255.255.255.0\","
               "\"gw\":\"192.168.0.1\","
               "\"urc\":0"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_failed_attempt) // NOLINT
{
    const network_info_str_t network_info = {
        { "0" },
        { "0" },
        { "0" },
    };
    const wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_generate(&ssid, &network_info, UPDATE_FAILED_ATTEMPT);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"urc\":1"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_failed_attempt_2) // NOLINT
{
    const network_info_str_t network_info = {
        { "192.168.0.50" },
        { "192.168.0.1" },
        { "255.255.255.0" },
    };
    const wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_generate(&ssid, &network_info, UPDATE_FAILED_ATTEMPT);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"192.168.0.50\","
               "\"netmask\":\"255.255.255.0\","
               "\"gw\":\"192.168.0.1\","
               "\"urc\":1"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_user_disconnect) // NOLINT
{
    const network_info_str_t network_info = {
        { "0" },
        { "0" },
        { "0" },
    };
    const wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_generate(&ssid, &network_info, UPDATE_USER_DISCONNECT);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"urc\":2"
               "}\n"),
        json_str);
}

TEST_F(TestJsonNetworkInfo, test_generate_lost_connection) // NOLINT
{
    const network_info_str_t network_info = {
        { "0" },
        { "0" },
        { "0" },
    };
    const wifi_ssid_t ssid = { "test_ssid" };
    json_network_info_generate(&ssid, &network_info, UPDATE_LOST_CONNECTION);
    string json_str = json_network_info_get();
    ASSERT_EQ(
        string("{"
               "\"ssid\":\"test_ssid\","
               "\"ip\":\"0\","
               "\"netmask\":\"0\","
               "\"gw\":\"0\","
               "\"urc\":3"
               "}\n"),
        json_str);
}
