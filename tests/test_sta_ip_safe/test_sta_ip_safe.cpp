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
    MainTaskCmd_sta_ip_safe_mutex_get,
    MainTaskCmd_sta_ip_safe_init,
    MainTaskCmd_sta_ip_safe_deinit,
    MainTaskCmd_sta_ip_safe_lock,
    MainTaskCmd_sta_ip_safe_unlock,
    MainTaskCmd_sta_ip_safe_get,
    MainTaskCmd_sta_ip_safe_set,
    MainTaskCmd_sta_ip_safe_reset,
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
    void
    SetUp() override
    {
        g_pTestClass = this;
        sem_init(&semaFreeRTOS, 0, 0);
        pid_test      = pthread_self();
        const int err = pthread_create(&pid_freertos, nullptr, &freertosStartup, this);
        assert(0 == err);
        while (0 != sem_wait(&semaFreeRTOS))
        {
        }
        esp_log_wrapper_init();
    }

    void
    TearDown() override
    {
        cmdQueue.push_and_wait(MainTaskCmd_Exit);
        sleep(1);
        vTaskEndScheduler();
        void *ret_code = nullptr;
        pthread_join(pid_freertos, &ret_code);
        sem_destroy(&semaFreeRTOS);
        esp_log_wrapper_deinit();
        g_pTestClass = nullptr;
    }

public:
    pthread_t             pid_test;
    pthread_t             pid_freertos;
    sem_t                 semaFreeRTOS;
    TQueue<MainTaskCmd_e> cmdQueue;
    os_mutex_t            result_sta_ip_safe_mutex_get;
    sta_ip_string_t       result_sta_ip_safe_get;
    sta_ip_address_t      param_sta_ip_safe_set;

    TestStaIpSafe();

    ~TestStaIpSafe() override;
};

TestStaIpSafe::TestStaIpSafe()
    : Test()
    , pid_test()
    , pid_freertos()
    , semaFreeRTOS()
{
}

TestStaIpSafe::~TestStaIpSafe() = default;

extern "C" {

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
    if (nullptr == g_pTestClass)
    {
        return false;
    }
    if (g_flagDisableCheckIsThreadFreeRTOS)
    {
        return true;
    }
    const pthread_t cur_thread_pid = pthread_self();
    if (cur_thread_pid == g_pTestClass->pid_test)
    {
        return false;
    }
    return true;
}

const char *
os_task_get_name(void)
{
    static const char g_task_name[] = "main";
    return const_cast<char *>(g_task_name);
}

unsigned int
lwip_port_rand(void)
{
    return (unsigned int)random();
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
        switch (cmd)
        {
            case MainTaskCmd_Exit:
                flagExit = true;
                break;
            case MainTaskCmd_sta_ip_safe_mutex_get:
                pTestStaIpSafe->result_sta_ip_safe_mutex_get = sta_ip_safe_mutex_get();
                break;
            case MainTaskCmd_sta_ip_safe_init:
                sta_ip_safe_init();
                break;
            case MainTaskCmd_sta_ip_safe_deinit:
                sta_ip_safe_deinit();
                break;
            case MainTaskCmd_sta_ip_safe_lock:
                sta_ip_safe_lock();
                break;
            case MainTaskCmd_sta_ip_safe_unlock:
                sta_ip_safe_unlock();
                break;
            case MainTaskCmd_sta_ip_safe_get:
                pTestStaIpSafe->result_sta_ip_safe_get = sta_ip_safe_get();
                break;
            case MainTaskCmd_sta_ip_safe_set:
                sta_ip_safe_set(pTestStaIpSafe->param_sta_ip_safe_set);
                break;
            case MainTaskCmd_sta_ip_safe_reset:
                sta_ip_safe_reset();
                break;
            default:
                printf("Error: Unknown cmd %d\n", (int)cmd);
                exit(1);
                break;
        }
        pTestStaIpSafe->cmdQueue.notify_handled();
    }
    vTaskDelete(nullptr);
}

static void *
freertosStartup(void *arg)
{
    auto *pObj = static_cast<TestStaIpSafe *>(arg);
    disableCheckingIfCurThreadIsFreeRTOS();
    BaseType_t res = xTaskCreate(
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

#define TEST_CHECK_LOG_RECORD(level_, msg_) ESP_LOG_WRAPPER_TEST_CHECK_LOG_RECORD("wifi_manager", level_, msg_)

/*** Unit-Tests *******************************************************************************************************/

TEST_F(TestStaIpSafe, test_all) // NOLINT
{
    // Test init/deinit
    {
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_init);

        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_get);
        ASSERT_EQ(string("0.0.0.0"), string(this->result_sta_ip_safe_get.buf));

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_deinit);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test lock/unlock without init
    {
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_lock);
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_unlock);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test sta_ip_safe_init / sta_ip_safe_deinit twice
    {
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_init);

        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_init);

        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_get);
        ASSERT_EQ(string("0.0.0.0"), string(this->result_sta_ip_safe_get.buf));

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_deinit);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_deinit);

        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test sta_ip_safe_set / sta_ip_safe_get
    {
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_init);
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        this->param_sta_ip_safe_set = sta_ip_safe_conv_str_to_ip("192.168.1.10");
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_set);
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 192.168.1.10");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_get);
        ASSERT_EQ(string("192.168.1.10"), string(this->result_sta_ip_safe_get.buf));

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_deinit);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test sta_ip_safe_set / sta_ip_safe_get without init
    {
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        this->param_sta_ip_safe_set = sta_ip_safe_conv_str_to_ip("192.168.1.10");
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_set);
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 192.168.1.10");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_get);
        ASSERT_EQ(string("192.168.1.10"), string(this->result_sta_ip_safe_get.buf));

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }

    // Test sta_ip_safe_reset
    {
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_init);
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        this->param_sta_ip_safe_set = sta_ip_safe_conv_str_to_ip("192.168.1.10");
        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_set);
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 192.168.1.10");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_get);
        ASSERT_EQ(string("192.168.1.10"), string(this->result_sta_ip_safe_get.buf));

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_reset);
        TEST_CHECK_LOG_RECORD(ESP_LOG_INFO, "Set STA IP String to: 0.0.0.0");
        ASSERT_TRUE(esp_log_wrapper_is_empty());

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_get);
        ASSERT_EQ(string("0.0.0.0"), string(this->result_sta_ip_safe_get.buf));

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_deinit);

        cmdQueue.push_and_wait(MainTaskCmd_sta_ip_safe_mutex_get);
        ASSERT_TRUE(nullptr != this->result_sta_ip_safe_mutex_get);

        ASSERT_TRUE(esp_log_wrapper_is_empty());
    }
}
