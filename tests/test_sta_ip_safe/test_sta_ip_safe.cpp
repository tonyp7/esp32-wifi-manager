/**
 * @file test_sta_ip_safe.cpp
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include <cstdio>
#include <semaphore.h>
#include "gtest/gtest.h"
#include "TQueue.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "os_mutex.h"
#include "sta_ip_safe.h"
#include "esp_log_wrapper.hpp"

using namespace std;

typedef enum MainTaskCmd_Tag
{
    MainTaskCmd_Exit,
} MainTaskCmd_e;

/*** Google-test class implementation *********************************************************************************/

class TestStaIpSafe;

static TestStaIpSafe *g_pTestClass;

static void *
freertosStartup(void *arg);

class TestStaIpSafe : public ::testing::Test
{
private:
protected:
    pthread_t pid;

    void
    SetUp() override
    {
        this->flag_mutex_create_fail = false;
        sem_init(&semaFreeRTOS, 0, 0);
        const int err = pthread_create(&pid, nullptr, &freertosStartup, this);
        assert(0 == err);
        while (0 != sem_wait(&semaFreeRTOS))
        {
        }
        esp_log_wrapper_init();
        g_pTestClass = this;
    }

    void
    TearDown() override
    {
        esp_log_wrapper_deinit();
        cmdQueue.push_and_wait(MainTaskCmd_Exit);
        vTaskEndScheduler();
        void *ret_code = nullptr;
        pthread_join(pid, &ret_code);
        sem_destroy(&semaFreeRTOS);
        g_pTestClass = nullptr;
    }

public:
    sem_t                 semaFreeRTOS;
    TQueue<MainTaskCmd_e> cmdQueue;
    bool                  flag_mutex_create_fail;

    TestStaIpSafe();

    ~TestStaIpSafe() override;
};

TestStaIpSafe::TestStaIpSafe()
    : Test()
    , pid()
    , semaFreeRTOS()
    , flag_mutex_create_fail(false)
{
}

TestStaIpSafe::~TestStaIpSafe() = default;

extern "C" {

os_mutex_t
os_mutex_create(void)
{
    if (g_pTestClass->flag_mutex_create_fail)
    {
        return nullptr;
    }
    SemaphoreHandle_t h_mutex = xSemaphoreCreateMutex();
    return h_mutex;
}

void
os_mutex_delete(os_mutex_t *p_mutex)
{
    if (nullptr != *p_mutex)
    {
        vSemaphoreDelete(*p_mutex);
        *p_mutex = nullptr;
    }
}

bool
os_mutex_lock_with_timeout(os_mutex_t h_mutex, TickType_t ticks_to_wait)
{
    if (pdTRUE != xSemaphoreTake(h_mutex, ticks_to_wait))
    {
        return false;
    }
    return true;
}

void
os_mutex_unlock(os_mutex_t h_mutex)
{
    xSemaphoreGive(h_mutex);
}

} // extern "C"

/*** Cmd-handler task *************************************************************************************************/

static void
cmdHandlerTask(void *parameters)
{
    auto *pTestStaIpSafe = static_cast<TestStaIpSafe *>(parameters);
    bool  flagExit       = false;
    sem_post(&pTestStaIpSafe->semaFreeRTOS);
    while (!flagExit)
    {
        const MainTaskCmd_e cmd = pTestStaIpSafe->cmdQueue.pop();
        if (MainTaskCmd_Exit == cmd)
        {
            flagExit = true;
        }
        else
        {
            printf("Error: Unknown cmd %d\n", (int)cmd);
            exit(1);
        }
        pTestStaIpSafe->cmdQueue.notify_handled();
    }
    vTaskDelete(nullptr);
}

static void *
freertosStartup(void *arg)
{
    auto *     pObj = static_cast<TestStaIpSafe *>(arg);
    BaseType_t res  = xTaskCreate(
        cmdHandlerTask,
        "cmdHandlerTask",
        configMINIMAL_STACK_SIZE,
        pObj,
        (tskIDLE_PRIORITY + 1),
        (xTaskHandle *)nullptr);
    assert(pdPASS == res);
    vTaskStartScheduler();
    return nullptr;
}

#define TEST_CHECK_LOG_RECORD(level_, msg_) \
    do \
    { \
        ASSERT_FALSE(esp_log_wrapper_is_empty()); \
        const LogRecord log_record = esp_log_wrapper_pop(); \
        ASSERT_EQ(level_, log_record.level); \
        ASSERT_EQ(string("wifi_manager"), log_record.tag); \
        ASSERT_EQ(string(msg_), log_record.message); \
    } while (0)

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestStaIpSafe, test_all) // NOLINT
{
    // Test init/deinit
    {
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        ASSERT_TRUE(sta_ip_safe_init());

        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        ASSERT_TRUE(nullptr != sta_ip_safe_mutex_get());
        const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
        ASSERT_EQ(string("0.0.0.0"), string(ip_str.buf));
        sta_ip_safe_deinit();
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test lock/unlock without init
    {
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        ASSERT_FALSE(sta_ip_safe_lock((TickType_t)0));
        sta_ip_safe_unlock();
        TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "sta_ip_safe_lock: Mutex is not initialized");
        TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "sta_ip_safe_unlock: Mutex is not initialized");
        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test init failed because of mutex create failed
    {
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        this->flag_mutex_create_fail = true;
        ASSERT_FALSE(sta_ip_safe_init());
        this->flag_mutex_create_fail = false;

        TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "sta_ip_safe_init: Failed to create mutex");
        ASSERT_TRUE(esp_log_wrapper_is_empty());
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
    }

    // Test sta_ip_safe_init / sta_ip_safe_deinit twice
    {
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        ASSERT_TRUE(sta_ip_safe_init());
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        ASSERT_TRUE(nullptr != sta_ip_safe_mutex_get());

        ASSERT_FALSE(sta_ip_safe_init());
        TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "sta_ip_safe_init: Mutex was already initialized");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
        ASSERT_EQ(string("0.0.0.0"), string(ip_str.buf));
        sta_ip_safe_deinit();
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        sta_ip_safe_deinit();
        TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "sta_ip_safe_lock: Mutex is not initialized");
        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test sta_ip_safe_set / sta_ip_safe_get
    {
        ASSERT_TRUE(sta_ip_safe_init());
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        const sta_ip_address_t ip_address = sta_ip_safe_conv_str_to_ip("192.168.1.10");
        ASSERT_TRUE(sta_ip_safe_set(ip_address, (TickType_t)0));
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 192.168.1.10");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
        ASSERT_EQ(string("192.168.1.10"), string(ip_str.buf));
        sta_ip_safe_deinit();
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test sta_ip_safe_set / sta_ip_safe_get without init
    {
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        const sta_ip_address_t ip_address = sta_ip_safe_conv_str_to_ip("192.168.1.10");
        ASSERT_FALSE(sta_ip_safe_set(ip_address, (TickType_t)0));
        TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "sta_ip_safe_lock: Mutex is not initialized");
        TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "sta_ip_safe_set: Timeout waiting mutex");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
        ASSERT_EQ(string(""), string(ip_str.buf));
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        TEST_CHECK_LOG_RECORD(ESP_LOG_ERROR, "sta_ip_safe_lock: Mutex is not initialized");
        TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "sta_ip_safe_get: Timeout waiting mutex");
        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test sta_ip_safe_reset
    {
        ASSERT_TRUE(sta_ip_safe_init());
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        const sta_ip_address_t ip_address = sta_ip_safe_conv_str_to_ip("192.168.1.10");
        ASSERT_TRUE(sta_ip_safe_set(ip_address, (TickType_t)0));
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 192.168.1.10");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
        ASSERT_EQ(string("192.168.1.10"), string(ip_str.buf));

        ASSERT_TRUE(sta_ip_safe_reset(0));
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        const sta_ip_string_t ip_str2 = sta_ip_safe_get((TickType_t)0);
        ASSERT_EQ(string("0.0.0.0"), string(ip_str2.buf));
        sta_ip_safe_deinit();
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test mutex timeout
    {
        {
            ASSERT_TRUE(sta_ip_safe_init());
            TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
            ASSERT_TRUE(esp_log_wrapper_is_empty());

            const sta_ip_address_t ip_address = sta_ip_safe_conv_str_to_ip("192.168.1.10");
            ASSERT_TRUE(sta_ip_safe_set(ip_address, (TickType_t)0));
            TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 192.168.1.10");
            ASSERT_TRUE(esp_log_wrapper_is_empty());

            const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
            ASSERT_EQ(string("192.168.1.10"), string(ip_str.buf));
            ASSERT_TRUE(esp_log_wrapper_is_empty());
        }

        // lock mutex before sta_ip_safe_set
        {
            ASSERT_TRUE(sta_ip_safe_lock((TickType_t)0));
            const sta_ip_address_t ip_address = sta_ip_safe_conv_str_to_ip("192.168.2.11");
            ASSERT_FALSE(sta_ip_safe_set(ip_address, (TickType_t)0));
            TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "sta_ip_safe_set: Timeout waiting mutex");
            ASSERT_TRUE(esp_log_wrapper_is_empty());

            sta_ip_safe_unlock();
            const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
            ASSERT_EQ(string("192.168.1.10"), string(ip_str.buf));
            ASSERT_TRUE(esp_log_wrapper_is_empty());
        }

        {
            const sta_ip_address_t ip_address = sta_ip_safe_conv_str_to_ip("192.168.2.11");
            ASSERT_TRUE(sta_ip_safe_set(ip_address, (TickType_t)0));
            TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 192.168.2.11");
            ASSERT_TRUE(esp_log_wrapper_is_empty());

            const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
            ASSERT_EQ(string("192.168.2.11"), string(ip_str.buf));
            ASSERT_TRUE(esp_log_wrapper_is_empty());
        }

        // lock mutex before sta_ip_safe_get
        {
            ASSERT_TRUE(sta_ip_safe_lock((TickType_t)0));
            const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
            TEST_CHECK_LOG_RECORD(ESP_LOG_WARN, "sta_ip_safe_get: Timeout waiting mutex");
            ASSERT_TRUE(esp_log_wrapper_is_empty());

            ASSERT_EQ(string(""), string(ip_str.buf));
            sta_ip_safe_unlock();
            ASSERT_TRUE(esp_log_wrapper_is_empty());
        }

        {
            const sta_ip_string_t ip_str = sta_ip_safe_get((TickType_t)0);
            ASSERT_EQ(string("192.168.2.11"), string(ip_str.buf));
            ASSERT_TRUE(esp_log_wrapper_is_empty());
        }

        sta_ip_safe_deinit();
        ASSERT_TRUE(nullptr == sta_ip_safe_mutex_get());
    }
}
