/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file http_server.c
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "http_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <esp_task_wdt.h>
#include "lwip/netbuf.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/opt.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/priv/api_msg.h"

#include "wifi_manager_internal.h"
#include "wifi_manager.h"
#include "json_access_points.h"

#include "cJSON.h"
#include "os_task.h"
#include "os_malloc.h"
#include "http_req.h"
#include "esp_type_wrapper.h"
#include "os_signal.h"
#include "os_mutex.h"
#include "os_sema.h"
#include "os_timer_sig.h"
#include "wifiman_msg.h"
#include "http_server_accept_and_handle_conn.h"
#include "time_units.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

typedef enum http_server_sig_e
{
    HTTP_SERVER_SIG_STOP               = OS_SIGNAL_NUM_0,
    HTTP_SERVER_SIG_TASK_WATCHDOG_FEED = OS_SIGNAL_NUM_1,
    HTTP_SERVER_SIG_USER_REQ_1         = OS_SIGNAL_NUM_2,
    HTTP_SERVER_SIG_USER_REQ_2         = OS_SIGNAL_NUM_3,
} http_server_sig_e;

#define HTTP_SERVER_SIG_FIRST (HTTP_SERVER_SIG_STOP)
#define HTTP_SERVER_SIG_LAST  (HTTP_SERVER_SIG_USER_REQ_1)

#define HTTP_SERVER_ACCEPT_TIMEOUT_MS (1 * 1000)

#define HTTP_SERVER_STATUS_JSON_REQUEST_TIMEOUT_MS (20 * 1000)
#define HTTP_SERVER_STA_AP_TIMEOUT_MS              (60 * 1000)

#define HTTP_SERVER_TASK_WDOG_MAX_FEED_INTERVAL_MS (1000U)
#define HTTP_SERVER_TASK_WDOG_MIN_FEED_FREQ        (3U)

/**
 * @brief RTOS task for the HTTP server. Do not start manually.
 * @see void http_server_start()
 */
static void
http_server_task(void);

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "http_server";

static os_mutex_static_t  g_http_server_mutex_mem;
static os_mutex_t         g_http_server_mutex;
static os_signal_static_t g_http_server_signal_mem;
static os_signal_t *      g_p_http_server_sig;
static os_sema_t          g_p_http_server_sema_send;
static os_sema_static_t   g_http_server_sema_send_mem;
struct netconn *          g_p_conn_listen;
static os_delta_ticks_t   g_timestamp_last_http_status_request;
static bool               g_is_ap_sta_ip_assigned;
static bool               g_http_server_disable_ap_stopping_by_timeout;

static os_timer_sig_periodic_t *      g_p_http_server_timer_sig_watchdog_feed;
static os_timer_sig_periodic_static_t g_http_server_timer_sig_watchdog_feed_mem;

ATTR_PURE
static os_signal_num_e
http_server_conv_to_sig_num(const http_server_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static http_server_sig_e
http_server_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)HTTP_SERVER_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)HTTP_SERVER_SIG_LAST));
    return (http_server_sig_e)sig_num;
}

void
http_server_init(void)
{
    assert(NULL == g_http_server_mutex);
    g_http_server_mutex = os_mutex_create_static(&g_http_server_mutex_mem);
    g_p_http_server_sig = os_signal_create_static(&g_http_server_signal_mem);
    os_signal_add(g_p_http_server_sig, http_server_conv_to_sig_num(HTTP_SERVER_SIG_STOP));
    os_signal_add(g_p_http_server_sig, http_server_conv_to_sig_num(HTTP_SERVER_SIG_TASK_WATCHDOG_FEED));
    os_signal_add(g_p_http_server_sig, http_server_conv_to_sig_num(HTTP_SERVER_SIG_USER_REQ_1));
    os_signal_add(g_p_http_server_sig, http_server_conv_to_sig_num(HTTP_SERVER_SIG_USER_REQ_2));
    g_p_http_server_sema_send = os_sema_create_static(&g_http_server_sema_send_mem);
}

void
http_server_sema_send_wait_immediate(void)
{
    os_sema_wait_immediate(g_p_http_server_sema_send);
}

bool
http_server_sema_send_wait_timeout(const uint32_t send_timeout_ms)
{
    return os_sema_wait_with_timeout(
        g_p_http_server_sema_send,
        (0 != send_timeout_ms) ? pdMS_TO_TICKS(send_timeout_ms) : OS_DELTA_TICKS_INFINITE);
}

void
http_server_disable_ap_stopping_by_timeout(void)
{
    g_http_server_disable_ap_stopping_by_timeout = true;
}

void
http_server_start(void)
{
    LOG_INFO("Start HTTP-Server");
    assert(NULL != g_http_server_mutex);

    os_mutex_lock(g_http_server_mutex);
    if (os_signal_is_any_thread_registered(g_p_http_server_sig))
    {
        LOG_WARN("Another HTTP-Server is already working");
        os_mutex_unlock(g_http_server_mutex);
    }

    os_mutex_unlock(g_http_server_mutex);

    const uint32_t stack_depth = 20U * 1024U;
    if (!os_task_create_finite_without_param(
            &http_server_task,
            "http_server",
            stack_depth,
            WIFI_MANAGER_TASK_PRIORITY - 1))
    {
        LOG_ERR("xTaskCreate failed: http_server");
    }
}

ATTR_UNUSED
void
http_server_stop(void)
{
    assert(NULL != g_http_server_mutex);
    os_mutex_lock(g_http_server_mutex);
    if (os_signal_is_any_thread_registered(g_p_http_server_sig))
    {
        LOG_INFO("Send request to stop HTTP-Server");
        if (!os_signal_send(g_p_http_server_sig, http_server_conv_to_sig_num(HTTP_SERVER_SIG_STOP)))
        {
            LOG_ERR("Failed to send HTTP-Server stop request");
        }
    }
    else
    {
        LOG_INFO("Send request to stop HTTP-Server, but HTTP-Server is not running");
    }
    os_mutex_unlock(g_http_server_mutex);
}

void
http_server_user_req(const http_server_user_req_code_e req_code)
{
    assert(NULL != g_http_server_mutex);
    os_mutex_lock(g_http_server_mutex);
    if (os_signal_is_any_thread_registered(g_p_http_server_sig))
    {
        http_server_sig_e sig_num = HTTP_SERVER_SIG_STOP;
        switch (req_code)
        {
            case HTTP_SERVER_USER_REQ_CODE_1:
                sig_num = HTTP_SERVER_SIG_USER_REQ_1;
                break;
            case HTTP_SERVER_USER_REQ_CODE_2:
                sig_num = HTTP_SERVER_SIG_USER_REQ_2;
                break;
        }
        if (HTTP_SERVER_SIG_STOP == sig_num)
        {
            LOG_ERR("Unknown req_code=%d", (printf_int_t)sig_num);
        }
        else
        {
            LOG_INFO("Send user request to HTTP-Server: sig_num=%d", (printf_int_t)sig_num);
            if (!os_signal_send(g_p_http_server_sig, http_server_conv_to_sig_num(sig_num)))
            {
                LOG_ERR("Failed to send the user request to HTTP-Server");
            }
        }
    }
    else
    {
        LOG_ERR("Failed to send the user request to HTTP-Server - HTTP-Server is not working");
    }
    os_mutex_unlock(g_http_server_mutex);
}

static bool
http_server_sig_register_cur_thread(void)
{
    os_mutex_lock(g_http_server_mutex);
    if (!os_signal_register_cur_thread(g_p_http_server_sig))
    {
        os_mutex_unlock(g_http_server_mutex);
        return false;
    }
    os_mutex_unlock(g_http_server_mutex);
    return true;
}

static void
http_server_sig_unregister_cur_thread(void)
{
    os_mutex_lock(g_http_server_mutex);
    os_signal_unregister_cur_thread(g_p_http_server_sig);
    os_mutex_unlock(g_http_server_mutex);
}

void
http_server_update_last_http_status_request(void)
{
    g_timestamp_last_http_status_request = xTaskGetTickCount();
}

static bool
http_server_is_status_json_timeout_expired(const uint32_t timeout_ms)
{
    if ((xTaskGetTickCount() - g_timestamp_last_http_status_request) > OS_DELTA_MS_TO_TICKS(timeout_ms))
    {
        return true;
    }
    return false;
}

void
http_server_on_ap_sta_connected(void)
{
    g_is_ap_sta_ip_assigned = false;
    http_server_update_last_http_status_request();
    LOG_DBG("http_server_on_ap_sta_connected: %lu", (printf_ulong_t)g_timestamp_last_http_status_request);
}

void
http_server_on_ap_sta_disconnected(void)
{
    g_is_ap_sta_ip_assigned = false;
    http_server_update_last_http_status_request();
    LOG_DBG("http_server_on_ap_sta_disconnected: %lu", (printf_ulong_t)g_timestamp_last_http_status_request);
}

void
http_server_on_ap_sta_ip_assigned(void)
{
    g_is_ap_sta_ip_assigned = true;
    http_server_update_last_http_status_request();
    LOG_DBG("http_server_on_ap_sta_ip_assigned: %lu", (printf_ulong_t)g_timestamp_last_http_status_request);
}

static bool
http_server_check_if_configuring_complete_wifi(
    bool *const            p_is_network_connected,
    bool *const            p_is_ap_sta_ip_assigned,
    const os_delta_ticks_t time_for_processing_request)
{
    if (!*p_is_network_connected)
    {
        *p_is_network_connected = true;
        http_server_update_last_http_status_request();
    }
    if ((!*p_is_ap_sta_ip_assigned) && wifi_manager_is_ap_sta_ip_assigned())
    {
        *p_is_ap_sta_ip_assigned = true;
        http_server_update_last_http_status_request();
    }
    else if ((*p_is_ap_sta_ip_assigned) && (!wifi_manager_is_ap_sta_ip_assigned()))
    {
        *p_is_ap_sta_ip_assigned = false;
        http_server_update_last_http_status_request();
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
    if (*p_is_ap_sta_ip_assigned)
    {
        const uint32_t timeout_ms = HTTP_SERVER_STATUS_JSON_REQUEST_TIMEOUT_MS;
        if ((!g_http_server_disable_ap_stopping_by_timeout)
            && http_server_is_status_json_timeout_expired(timeout_ms + time_for_processing_request))
        {
            LOG_INFO("There are no HTTP requests for status.json while AP_STA is connected for %u ms", timeout_ms);
            return true;
        }
    }
    else
    {
        const uint32_t timeout_ms = HTTP_SERVER_STA_AP_TIMEOUT_MS;
        if ((!g_http_server_disable_ap_stopping_by_timeout)
            && http_server_is_status_json_timeout_expired(timeout_ms + time_for_processing_request))
        {
            LOG_INFO("There are no HTTP requests for status.json while AP_STA is not connected for %u ms", timeout_ms);
            return true;
        }
    }
    return false;
}

static bool
http_server_check_if_configuring_complete_ethernet(
    bool *const            p_is_network_connected,
    const os_delta_ticks_t time_for_processing_request)
{
    if (!*p_is_network_connected)
    {
        *p_is_network_connected = true;
        http_server_update_last_http_status_request();
    }
    if ((!g_http_server_disable_ap_stopping_by_timeout)
        && http_server_is_status_json_timeout_expired(
            HTTP_SERVER_STATUS_JSON_REQUEST_TIMEOUT_MS + time_for_processing_request))
    {
        LOG_INFO("There are no HTTP requests for status.json for %u ms", HTTP_SERVER_STATUS_JSON_REQUEST_TIMEOUT_MS);
        return true;
    }
    return false;
}

static bool
http_server_check_if_configuring_complete(const os_delta_ticks_t time_for_processing_request)
{
    static bool g_is_network_connected = false;

    if (wifi_manager_is_connected_to_wifi())
    {
        return http_server_check_if_configuring_complete_wifi(
            &g_is_network_connected,
            &g_is_ap_sta_ip_assigned,
            time_for_processing_request);
    }
    if (wifi_manager_is_connected_to_ethernet())
    {
        return http_server_check_if_configuring_complete_ethernet(&g_is_network_connected, time_for_processing_request);
    }
    g_is_network_connected  = false;
    g_is_ap_sta_ip_assigned = false;
    return false;
}

static void
http_server_task_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_sig_periodic_start(g_p_http_server_timer_sig_watchdog_feed);
}

static void
http_server_task_wdt_reset(void)
{
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

static void
http_server_netconn_callback(const struct netconn *const p_conn, const enum netconn_evt event)
{
    if (p_conn != g_p_conn_listen)
    {
        switch (event)
        {
            case NETCONN_EVT_SENDPLUS:
                os_sema_signal(g_p_http_server_sema_send);
                break;
            case NETCONN_EVT_ERROR:
                LOG_DBG("NETCONN_EVT_ERROR");
                os_sema_signal(g_p_http_server_sema_send);
                break;
            default:
                break;
        }
    }
}

static void
http_server_netconn_callback_wrap(struct netconn *p_conn, enum netconn_evt event, ATTR_UNUSED u16_t len)
{
    http_server_netconn_callback(p_conn, event);
}

static bool
http_server_handle_sig_events(os_signal_events_t *const p_sig_events)
{
    bool flag_stop = false;
    for (;;)
    {
        const os_signal_num_e sig_num = os_signal_num_get_next(p_sig_events);
        if (OS_SIGNAL_NUM_NONE == sig_num)
        {
            break;
        }
        const http_server_sig_e http_server_task_sig = http_server_conv_from_sig_num(sig_num);
        switch (http_server_task_sig)
        {
            case HTTP_SERVER_SIG_STOP:
                LOG_INFO("Got signal STOP");
                flag_stop = true;
                break;
            case HTTP_SERVER_SIG_TASK_WATCHDOG_FEED:
                http_server_task_wdt_reset();
                break;
            case HTTP_SERVER_SIG_USER_REQ_1:
                LOG_INFO("Got signal USER_REQ_1");
                wifi_manager_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_1);
                break;
            case HTTP_SERVER_SIG_USER_REQ_2:
                LOG_INFO("Got signal USER_REQ_2");
                wifi_manager_cb_on_user_req(HTTP_SERVER_USER_REQ_CODE_2);
                break;
        }
    }
    return flag_stop;
}

static uint32_t
http_server_get_task_wdog_feed_period_ms(void)
{
    const uint32_t period_ms = (CONFIG_ESP_TASK_WDT_TIMEOUT_S * TIME_UNITS_MS_PER_SECOND)
                               / HTTP_SERVER_TASK_WDOG_MIN_FEED_FREQ;
    if (period_ms > HTTP_SERVER_TASK_WDOG_MAX_FEED_INTERVAL_MS)
    {
        return HTTP_SERVER_TASK_WDOG_MAX_FEED_INTERVAL_MS;
    }
    return period_ms;
}

static bool
http_server_netconn_listen(struct netconn *const p_conn)
{
    const err_t err = netconn_listen(p_conn);
    if (ERR_OK != err)
    {
        LOG_ERR_ESP(err, "Can't open socket for HTTP Server");
        return false;
    }
    return true;
}

static void
http_server_task(void)
{
    LOG_INFO("HTTP-Server thread started");
    if (!http_server_sig_register_cur_thread())
    {
        LOG_WARN("Another HTTP-Server is already working - finish current thread");
        return;
    }

    struct netconn *p_conn = netconn_new_with_callback(NETCONN_TCP, &http_server_netconn_callback_wrap);
    if (NULL == p_conn)
    {
        LOG_ERR("Can't create netconn for HTTP Server");
        return;
    }
    g_p_conn_listen       = p_conn;
    const u16_t http_port = 80U;
    netconn_bind(p_conn, IP_ADDR_ANY, http_port);

    if (!http_server_netconn_listen(p_conn))
    {
        return;
    }

    netconn_set_recvtimeout(p_conn, HTTP_SERVER_ACCEPT_TIMEOUT_MS);
    LOG_INFO("HTTP Server listening on 80/tcp");

    LOG_INFO("TaskWatchdog: Create timer");
    g_p_http_server_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_http_server_timer_sig_watchdog_feed_mem,
        "http:wdog",
        g_p_http_server_sig,
        http_server_conv_to_sig_num(HTTP_SERVER_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(http_server_get_task_wdog_feed_period_ms()));

    http_server_task_wdt_add_and_start();

    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (os_signal_wait_with_timeout(g_p_http_server_sig, OS_DELTA_TICKS_IMMEDIATE, &sig_events))
        {
            const bool flag_stop = http_server_handle_sig_events(&sig_events);
            if (flag_stop)
            {
                break;
            }
        }

        const os_delta_ticks_t time_for_processing_request = http_server_accept_and_handle_conn(p_conn);

        if (wifi_manager_is_ap_active() && http_server_check_if_configuring_complete(time_for_processing_request)
            && wifi_manager_is_connected_to_wifi_or_ethernet())
        {
            LOG_INFO("Stop WiFi AP");
            wifi_manager_stop_ap();
        }
        taskYIELD(); /* allows the freeRTOS scheduler to take over if needed. */
    }
    LOG_INFO("Stop HTTP-Server");
    LOG_INFO("TaskWatchdog: Unregister current thread");
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());
    LOG_INFO("TaskWatchdog: Stop timer");
    os_timer_sig_periodic_stop(g_p_http_server_timer_sig_watchdog_feed);
    LOG_INFO("TaskWatchdog: Delete timer");
    os_timer_sig_periodic_delete(&g_p_http_server_timer_sig_watchdog_feed);
    LOG_INFO("Close socket");
    netconn_close(p_conn);
    netconn_delete(p_conn);
    http_server_sig_unregister_cur_thread();
}
