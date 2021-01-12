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

@file wifi_manager.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "wifi_manager.h"
#include "wifi_manager_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"

#include "json.h"
#include "http_server.h"
#include "tcpip_adapter.h"
#include "dns_server.h"
#include "json_network_info.h"
#include "json_access_points.h"
#include "../../main/includes/ethernet.h"
#include "sta_ip_safe.h"
#include "ap_ssid.h"
#include "wifiman_msg.h"
#include "access_points_list.h"
#include "wifi_sta_config.h"
#include "http_req.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifi_manager";

static SemaphoreHandle_t gh_wifi_json_mutex = NULL;

static uint16_t         g_wifi_ap_num = MAX_AP_NUM;
static wifi_ap_record_t g_wifi_accessp_records[MAX_AP_NUM];

static wifi_manager_cb_ptr g_wifi_cb_ptr_arr[MESSAGE_CODE_COUNT];

static wifi_manager_http_callback_t   g_wifi_cb_on_http_get;
static wifi_manager_http_cb_on_post_t g_wifi_cb_on_http_post;
static wifi_manager_http_callback_t   g_wifi_cb_on_http_delete;

/* @brief task handle for the main wifi_manager task */
static TaskHandle_t gh_wifi_manager_task = NULL;

static EventGroupHandle_t g_wifi_manager_event_group;

/* @brief indicate that the ESP32 is currently connected. */
#define WIFI_MANAGER_WIFI_CONNECTED_BIT (BIT0)

#define WIFI_MANAGER_AP_STA_CONNECTED_BIT (BIT1)

/* @brief Set automatically once the SoftAP is started */
#define WIFI_MANAGER_AP_STARTED_BIT (BIT2)

/* @brief When set, means a client requested to connect to an access point.*/
#define WIFI_MANAGER_REQUEST_STA_CONNECT_BIT (BIT3)

/* @brief This bit is set automatically as soon as a connection was lost */
#define WIFI_MANAGER_STA_DISCONNECT_BIT (BIT4)

/* @brief When set, means the wifi manager attempts to restore a previously saved connection at startup. */
#define WIFI_MANAGER_REQUEST_RESTORE_STA_BIT (BIT5)

/* @brief When set, means a client requested to disconnect from currently connected AP. */
#define WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT (BIT6)

/* @brief When set, means a scan is in progress */
#define WIFI_MANAGER_SCAN_BIT (BIT7)

/* @brief When set, means user requested for a disconnect */
#define WIFI_MANAGER_REQUEST_DISCONNECT_BIT (BIT8)

ATTR_NORETURN
static void
wifi_manager(const void *p_params);

void
wifi_manager_scan_async(void)
{
    wifiman_msg_send_cmd_start_wifi_scan();
}

void
wifi_manager_disconnect_async(void)
{
    wifiman_msg_send_cmd_disconnect_sta();
}

void
wifi_manager_start(
    const WiFiAntConfig_t *        p_wifi_ant_config,
    wifi_manager_http_callback_t   cb_on_http_get,
    wifi_manager_http_cb_on_post_t cb_on_http_post,
    wifi_manager_http_callback_t   cb_on_http_delete)
{
    g_wifi_cb_on_http_get    = cb_on_http_get;
    g_wifi_cb_on_http_post   = cb_on_http_post;
    g_wifi_cb_on_http_delete = cb_on_http_delete;

    http_server_init();
    dns_server_init();

    if (!wifiman_msg_init())
    {
        LOG_ERR("%s failed", "wifiman_msg_init");
        return;
    }
    /* memory allocation */
    gh_wifi_json_mutex = xSemaphoreCreateMutex();

    ESP_ERROR_CHECK(json_access_points_init());

    json_network_info_init();

    wifi_sta_config_init();

    for (message_code_e i = 0; i < MESSAGE_CODE_COUNT; ++i)
    {
        g_wifi_cb_ptr_arr[i] = NULL;
    }
    sta_ip_safe_init();
    g_wifi_manager_event_group = xEventGroupCreate();

    /* start wifi manager task */
    const char *   task_name   = "wifi_manager";
    const uint32_t stack_depth = 4096U;
    if (!os_task_create_with_const_param(
            &wifi_manager,
            task_name,
            stack_depth,
            (const void *)p_wifi_ant_config,
            WIFI_MANAGER_TASK_PRIORITY,
            &gh_wifi_manager_task))
    {
        LOG_ERR("Can't create thread: %s", task_name);
    }
}

http_server_resp_t
wifi_manager_cb_on_http_get(const char *p_path)
{
    if (NULL == g_wifi_cb_on_http_get)
    {
        return http_server_resp_404();
    }
    return g_wifi_cb_on_http_get(p_path);
}

http_server_resp_t
wifi_manager_cb_on_http_post(const char *p_path, const http_req_body_t http_body)
{
    if (NULL == g_wifi_cb_on_http_post)
    {
        return http_server_resp_404();
    }
    return g_wifi_cb_on_http_post(p_path, http_body.ptr);
}

http_server_resp_t
wifi_manager_cb_on_http_delete(const char *p_path)
{
    if (NULL == g_wifi_cb_on_http_delete)
    {
        return http_server_resp_404();
    }
    return g_wifi_cb_on_http_delete(p_path);
}

bool
wifi_manager_clear_sta_config(void)
{
    return wifi_sta_config_clear();
}

/**
 * @brief Generates the connection status json: ssid and IP addresses.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
static void
wifi_manager_generate_ip_info_json(const update_reason_code_e update_reason_code)
{
    tcpip_adapter_ip_info_t ip_info = { 0 };
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    const wifi_ssid_t ssid = wifi_sta_config_get_ssid();

    network_info_str_t ip_info_str = {
        .ip      = { "0" },
        .gw      = { "0" },
        .netmask = { "0" },
    };
    if (UPDATE_CONNECTION_OK == update_reason_code)
    {
        snprintf(ip_info_str.ip, sizeof(ip_info_str.ip), "%s", ip4addr_ntoa(&ip_info.ip));
        snprintf(ip_info_str.netmask, sizeof(ip_info_str.netmask), "%s", ip4addr_ntoa(&ip_info.netmask));
        snprintf(ip_info_str.gw, sizeof(ip_info_str.gw), "%s", ip4addr_ntoa(&ip_info.gw));
    }
    json_network_info_generate(&ssid, &ip_info_str, update_reason_code);
}

bool
wifi_manager_lock_json_buffer(const TickType_t ticks_to_wait)
{
    if (NULL == gh_wifi_json_mutex)
    {
        return false;
    }
    if (pdTRUE != xSemaphoreTake(gh_wifi_json_mutex, ticks_to_wait))
    {
        return false;
    }
    return true;
}

void
wifi_manager_unlock_json_buffer(void)
{
    xSemaphoreGive(gh_wifi_json_mutex);
}

static void
wifi_manager_event_handler(
    ATTR_UNUSED void *     p_ctx,
    const esp_event_base_t event_base,
    const int32_t          event_id,
    void *                 event_data)
{
    if (WIFI_EVENT == event_base)
    {
        switch (event_id)
        {
            case WIFI_EVENT_WIFI_READY:
                LOG_INFO("WIFI_EVENT_WIFI_READY");
                break;
            case WIFI_EVENT_SCAN_DONE:
                LOG_DBG("WIFI_EVENT_SCAN_DONE");
                xEventGroupClearBits(g_wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
                wifiman_msg_send_ev_scan_done();
                break;
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                LOG_INFO("WIFI_EVENT_STA_AUTHMODE_CHANGE");
                break;
            case WIFI_EVENT_AP_START:
                LOG_INFO("WIFI_EVENT_AP_START");
                xEventGroupSetBits(g_wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
                wifi_manager_scan_async();
                break;
            case WIFI_EVENT_AP_STOP:
                break;
            case WIFI_EVENT_AP_PROBEREQRECVED:
                break;
            case WIFI_EVENT_AP_STACONNECTED: /* a user disconnected from the SoftAP */
                LOG_INFO("WIFI_EVENT_AP_STACONNECTED");
                xEventGroupSetBits(g_wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                LOG_INFO("WIFI_EVENT_AP_STADISCONNECTED");
                xEventGroupClearBits(g_wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
                break;
            case WIFI_EVENT_STA_START:
                LOG_INFO("WIFI_EVENT_STA_START");
                break;
            case WIFI_EVENT_STA_STOP:
                LOG_INFO("WIFI_EVENT_STA_STOP");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                LOG_INFO("WIFI_EVENT_STA_CONNECTED");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
            {
                LOG_INFO("WIFI_EVENT_STA_DISCONNECTED");
                const wifi_event_sta_disconnected_t *p_disconnected = (const wifi_event_sta_disconnected_t *)event_data;
                wifiman_msg_send_ev_disconnected(p_disconnected->reason);
                break;
            }
            default:
                break;
        }
    }
    else if (IP_EVENT == event_base)
    {
        if (IP_EVENT_STA_GOT_IP == event_id)
        {
            LOG_INFO("IP_EVENT_STA_GOT_IP");
            const ip_event_got_ip_t *p_ip_event = (const ip_event_got_ip_t *)event_data;
            wifiman_msg_send_ev_got_ip(p_ip_event->ip_info.ip.addr);
        }
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
}

void
wifi_manager_connect_async(void)
{
    /* in order to avoid a false positive on the front end app we need to quickly flush the ip json
     * There'se a risk the front end sees an IP or a password error when in fact
     * it's a remnant from a previous connection
     */
    if (wifi_manager_lock_json_buffer(portMAX_DELAY))
    {
        json_network_info_clear();
        wifi_manager_unlock_json_buffer();
    }
    wifiman_msg_send_cmd_connect_sta(CONNECTION_REQUEST_USER);
}

/**
 * Frees up all memory allocated by the wifi_manager and kill the task.
 */
static void
wifi_manager_destroy(void)
{
    LOG_INFO("%s", __func__);
    if (NULL != gh_wifi_manager_task)
    {
        vTaskDelete(gh_wifi_manager_task);
        gh_wifi_manager_task = NULL;

        /* heap buffers */
        json_access_points_deinit();
        json_network_info_deinit();
        sta_ip_safe_deinit();

        /* RTOS objects */
        vSemaphoreDelete(gh_wifi_json_mutex);
        gh_wifi_json_mutex = NULL;
        vEventGroupDelete(g_wifi_manager_event_group);
        g_wifi_manager_event_group = NULL;
        wifiman_msg_deinit();
    }
}

void
wifi_manager_stop(void)
{
    LOG_INFO("%s", __func__);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler);

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    wifi_manager_destroy();

    tcpip_adapter_stop(TCPIP_ADAPTER_IF_STA);
    tcpip_adapter_stop(TCPIP_ADAPTER_IF_AP);
}

void
wifi_manager_set_callback(const message_code_e message_code, wifi_manager_cb_ptr func_ptr)
{
    if (message_code < MESSAGE_CODE_COUNT)
    {
        g_wifi_cb_ptr_arr[message_code] = func_ptr;
    }
}

static void
wifi_handle_ev_scan_done(void)
{
    LOG_DBG("MESSAGE: EVENT_SCAN_DONE");
    /* As input param, it stores max AP number ap_records can hold. As output param, it receives the
     * actual AP number this API returns.
     * As a consequence, ap_num MUST be reset to MAX_AP_NUM at every scan */
    g_wifi_ap_num = MAX_AP_NUM;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&g_wifi_ap_num, g_wifi_accessp_records));
    /* make sure the http server isn't trying to access the list while it gets refreshed */
    if (wifi_manager_lock_json_buffer(pdMS_TO_TICKS(1000)))
    {
        /* Will remove the duplicate SSIDs from the list and update ap_num */
        g_wifi_ap_num = ap_list_filter_unique(g_wifi_accessp_records, g_wifi_ap_num);
        json_access_points_generate(g_wifi_accessp_records, g_wifi_ap_num);
        wifi_manager_unlock_json_buffer();
    }
    else
    {
        LOG_ERR("could not get access to json mutex in wifi_scan");
    }

    http_server_start();
    dns_server_start();
}

static void
wifi_handle_cmd_start_wifi_scan(void)
{
    LOG_DBG("MESSAGE: ORDER_START_WIFI_SCAN");

    /* if a scan is already in progress this message is simply ignored thanks to the
     * WIFI_MANAGER_SCAN_BIT uxBit */
    const EventBits_t uxBits = xEventGroupGetBits(g_wifi_manager_event_group);
    if (0 == (uxBits & (uint32_t)WIFI_MANAGER_SCAN_BIT))
    {
        xEventGroupSetBits(g_wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
        /* wifi scanner config */
        const wifi_scan_config_t scan_config = {
            .ssid        = NULL,
            .bssid       = NULL,
            .channel     = 0,
            .show_hidden = true,
            .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
            .scan_time   = {
                .active  = {
                    .min = 0,
                    .max = 0,
                },
            },
        };

        const esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
        // sometimes when connecting to a network, a scan is started at the same time and then the scan
        // will fail that's fine because we already have a network to connect and we don't need new scan
        // results
        // TODO: maybe fix this in the web page that scanning is stopped when connecting to a network
        if (0 != ret)
        {
            LOG_WARN("scan start return: %d", ret);
        }
    }
}

static void
wifi_handle_cmd_load_sta(void)
{
    LOG_INFO("MESSAGE: ORDER_LOAD_AND_RESTORE_STA");
    if (wifi_sta_config_fetch())
    {
        LOG_INFO("Saved wifi found on startup. Will attempt to connect.");
        wifiman_msg_send_cmd_connect_sta(CONNECTION_REQUEST_RESTORE_CONNECTION);
    }
    else
    {
        /* no wifi saved: start soft AP! This is what should happen during a first run */
        LOG_INFO("No saved wifi found on startup. Starting access point.");
        wifiman_msg_send_cmd_start_ap();
    }
}

static void
wifi_handle_cmd_connect_sta(const wifiman_msg_param_t *p_param)
{
    LOG_INFO("MESSAGE: ORDER_CONNECT_STA");

    /* very important: precise that this connection attempt is specifically requested.
     * Param in that case is a boolean indicating if the request was made automatically
     * by the wifi_manager.
     * */
    const connection_request_made_by_code_e conn_req = wifiman_conv_param_to_conn_req(p_param);
    if (CONNECTION_REQUEST_USER == conn_req)
    {
        xEventGroupSetBits(g_wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
    }
    else if (CONNECTION_REQUEST_RESTORE_CONNECTION == conn_req)
    {
        xEventGroupSetBits(g_wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }

    const EventBits_t uxBits = xEventGroupGetBits(g_wifi_manager_event_group);
    if (0 != (uxBits & (uint32_t)WIFI_MANAGER_WIFI_CONNECTED_BIT))
    {
        wifiman_msg_send_cmd_disconnect_sta();
        /* todo: reconnect */
    }
    else
    {
        /* update config to latest and attempt connection */
        wifi_config_t wifi_config = wifi_sta_config_get_copy();
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

/**
 * @brief Handle event EVENT_STA_DISCONNECTED
 * @note this even can be posted in numerous different conditions
 *
 * 1. SSID password is wrong
 * 2. Manual disconnection ordered
 * 3. Connection lost
 *
 * Having clear understand as to WHY the event was posted is key to having an efficient wifi manager
 *
 * With wifi_manager, we determine:
 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, We consider it's a client that requested the
 *connection. When WIFI_EVENT_STA_DISCONNECTED is posted, it's probably a password/something went
 *wrong with the handshake.
 *
 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, it's a disconnection that was ASKED by the
 *client (clicking disconnect in the app) When WIFI_EVENT_STA_DISCONNECTED is posted, saved wifi is
 *erased from the NVS memory.
 *
 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT and WIFI_MANAGER_REQUEST_STA_CONNECT_BIT are NOT set,
 *it's a lost connection
 *
 *  In this version of the software, reason codes are not used. They are indicated here for
 *potential future usage.
 *
 *  REASON CODE:
 *  1       UNSPECIFIED
 *  2       AUTH_EXPIRE auth no longer valid, this smells like someone changed a password on the AP
 *  3       AUTH_LEAVE
 *  4       ASSOC_EXPIRE
 *  5       ASSOC_TOOMANY too many devices already connected to the AP => AP fails to respond
 *  6       NOT_AUTHED
 *  7       NOT_ASSOCED
 *  8       ASSOC_LEAVE
 *  9       ASSOC_NOT_AUTHED
 *  10      DISASSOC_PWRCAP_BAD
 *  11      DISASSOC_SUPCHAN_BAD
 *  12      <n/a>
 *  13      IE_INVALID
 *  14      MIC_FAILURE
 *  15      4WAY_HANDSHAKE_TIMEOUT wrong password! This was personnaly tested on my home wifi
 *          with a wrong password.
 *  16      GROUP_KEY_UPDATE_TIMEOUT
 *  17      IE_IN_4WAY_DIFFERS
 *  18      GROUP_CIPHER_INVALID
 *  19      PAIRWISE_CIPHER_INVALID
 *  20      AKMP_INVALID
 *  21      UNSUPP_RSN_IE_VERSION
 *  22      INVALID_RSN_IE_CAP
 *  23      802_1X_AUTH_FAILED  wrong password?
 *  24      CIPHER_SUITE_REJECTED
 *  200     BEACON_TIMEOUT
 *  201     NO_AP_FOUND
 *  202     AUTH_FAIL
 *  203     ASSOC_FAIL
 *  204     HANDSHAKE_TIMEOUT
 *
 *
 * @param p_param - pointer to wifiman_msg_param_t
 */
static bool
wifi_handle_ev_disconnected(const wifiman_msg_param_t *p_param)
{
    const wifiman_disconnection_reason_t reason = wifiman_conv_param_to_reason(p_param);
    LOG_INFO("MESSAGE: EVENT_STA_DISCONNECTED with Reason code: %d", reason);

    /* if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan
     * to never work again. For this reason SCAN_BIT is cleared too */
    const EventBits_t event_bits = xEventGroupClearBits(
        g_wifi_manager_event_group,
        WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_SCAN_BIT);

    /* reset saved sta IP */
    sta_ip_safe_reset(portMAX_DELAY);

    if (0 != (event_bits & (uint32_t)WIFI_MANAGER_REQUEST_STA_CONNECT_BIT))
    {
        LOG_INFO("event_bits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT");
        /* there are no retries when it's a user requested connection by design. This avoids a user
         * hanging too much in case they typed a wrong password for instance. Here we simply clear the
         * request bit and move on */
        xEventGroupClearBits(g_wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

        if (wifi_manager_lock_json_buffer(portMAX_DELAY))
        {
            wifi_manager_generate_ip_info_json(UPDATE_FAILED_ATTEMPT);
            wifi_manager_unlock_json_buffer();
        }
    }
    else if (0 != (event_bits & (uint32_t)WIFI_MANAGER_REQUEST_DISCONNECT_BIT))
    {
        LOG_INFO("event_bits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT");
        /* user manually requested a disconnect so the lost connection is a normal event. Clear the flag
         * and restart the AP */
        xEventGroupClearBits(g_wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

        if (wifi_manager_lock_json_buffer(portMAX_DELAY))
        {
            wifi_manager_generate_ip_info_json(UPDATE_USER_DISCONNECT);
            wifi_manager_unlock_json_buffer();
        }

        /* Erase configuration and save it ot NVS memory */
        wifi_sta_config_clear();

        /* start SoftAP */
        wifiman_msg_send_cmd_start_ap();
    }
    else
    {
        LOG_INFO("lost connection");
        /* lost connection ? */
        if (wifi_manager_lock_json_buffer(portMAX_DELAY))
        {
            wifi_manager_generate_ip_info_json(UPDATE_LOST_CONNECTION);
            wifi_manager_unlock_json_buffer();
        }

        wifiman_msg_send_cmd_connect_sta(CONNECTION_REQUEST_AUTO_RECONNECT);
    }
    if (0 == (event_bits & (uint32_t)WIFI_MANAGER_WIFI_CONNECTED_BIT))
    {
        return false;
    }
    return true;
}

static void
wifi_handle_cmd_start_ip(void)
{
    LOG_INFO("MESSAGE: ORDER_START_AP");
    esp_wifi_set_mode(WIFI_MODE_APSTA);
}

static void
wifi_handle_ev_got_ip(const wifiman_msg_param_t *p_param)
{
    LOG_INFO("MESSAGE: EVENT_STA_GOT_IP");

    const EventBits_t event_bits = xEventGroupClearBits(
        g_wifi_manager_event_group,
        WIFI_MANAGER_REQUEST_STA_CONNECT_BIT | WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
    xEventGroupSetBits(g_wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);

    /* save IP as a string for the HTTP server host */
    const sta_ip_address_t ip_addr = wifiman_conv_param_to_ip_addr(p_param);
    sta_ip_safe_set(ip_addr, portMAX_DELAY);

    /* save wifi config in NVS if it wasn't a restored of a connection */
    if (0 == (event_bits & (uint32_t)WIFI_MANAGER_REQUEST_RESTORE_STA_BIT))
    {
        wifi_sta_config_save();
    }

    /* refresh JSON with the new IP */
    if (wifi_manager_lock_json_buffer(portMAX_DELAY))
    {
        /* generate the connection info with success */
        wifi_manager_generate_ip_info_json(UPDATE_CONNECTION_OK);
        wifi_manager_unlock_json_buffer();
    }
    else
    {
        ESP_ERROR_CHECK(ESP_FAIL);
    }

    /* bring down DNS hijack */
    dns_server_stop();
}

static void
wifi_handle_cmd_disconnect_sta(void)
{
    LOG_INFO("MESSAGE: ORDER_DISCONNECT_STA");

    /* precise this is coming from a user request */
    xEventGroupSetBits(g_wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

    const esp_err_t err = esp_wifi_disconnect();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_disconnect");
    }
}

ATTR_NORETURN
static void
wifi_manager_main_loop(void)
{
    for (;;)
    {
        queue_message msg = { 0 };
        if (!wifiman_msg_recv(&msg))
        {
            LOG_ERR("%s failed", "wifiman_msg_recv");
            /**
             * wifiman_msg_recv calls xQueueReceive with infinite timeout,
             * so it should never return false and we should never get here,
             * but as a safety precaution to prevent 100% CPU usage we can sleep for a while to give time other threads.
             */
            const uint32_t delay_ms = 100U;
            vTaskDelay(delay_ms / portTICK_PERIOD_MS);
            continue;
        }
        bool flag_do_not_call_cb = false;
        switch (msg.code)
        {
            case EVENT_SCAN_DONE:
                wifi_handle_ev_scan_done();
                break;
            case ORDER_START_WIFI_SCAN:
                wifi_handle_cmd_start_wifi_scan();
                break;
            case ORDER_LOAD_AND_RESTORE_STA:
                wifi_handle_cmd_load_sta();
                break;
            case ORDER_CONNECT_STA:
                wifi_handle_cmd_connect_sta(&msg.msg_param);
                break;
            case EVENT_STA_DISCONNECTED:
                if (!wifi_handle_ev_disconnected(&msg.msg_param))
                {
                    flag_do_not_call_cb = true;
                }
                break;
            case ORDER_START_AP:
                wifi_handle_cmd_start_ip();
                break;
            case EVENT_STA_GOT_IP:
                wifi_handle_ev_got_ip(&msg.msg_param);
                break;
            case ORDER_DISCONNECT_STA:
                wifi_handle_cmd_disconnect_sta();
                break;
            default:
                break;
        }
        if (NULL != g_wifi_cb_ptr_arr[msg.code])
        {
            if (!flag_do_not_call_cb)
            {
                (*g_wifi_cb_ptr_arr[msg.code])(NULL);
            }
        }
    }
}

static void
wifi_manager_set_ant_config(const WiFiAntConfig_t *p_wifi_ant_config)
{
    if (NULL == p_wifi_ant_config)
    {
        return;
    }
    esp_err_t err = esp_wifi_set_ant_gpio(&p_wifi_ant_config->wifi_ant_gpio_config);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_wifi_set_ant_gpio failed");
    }
    err = esp_wifi_set_ant(&p_wifi_ant_config->wifi_ant_config);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_wifi_set_ant failed");
    }
}

static void
wifi_manager_generate_ssid_from_orig_and_mac(
    char *       p_ssid_str_buf,
    const size_t ssid_max_len,
    const char * p_orig_ap_ssid)
{
    ap_mac_t ap_mac = { 0 };
    ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_AP, &ap_mac.mac[0]));
    ap_ssid_generate(p_ssid_str_buf, ssid_max_len, p_orig_ap_ssid, &ap_mac);
}

static wifi_config_t
wifi_manager_generate_ap_config(const struct wifi_settings_t *p_wifi_settings)
{
    wifi_config_t ap_config = {
        .ap = {
            .ssid            = {
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            },
            .password        = {
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            },
            .ssid_len        = 0,
            .channel         = p_wifi_settings->ap_channel,
            .authmode        = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden     = p_wifi_settings->ap_ssid_hidden,
            .max_connection  = DEFAULT_AP_MAX_CONNECTIONS,
            .beacon_interval = DEFAULT_AP_BEACON_INTERVAL,
        },
    };

    wifi_manager_generate_ssid_from_orig_and_mac(
        (char *)&ap_config.ap.ssid[0],
        sizeof(ap_config.ap.ssid),
        (const char *)&p_wifi_settings->ap_ssid[0]);
    snprintf((char *)&ap_config.ap.password[0], sizeof(ap_config.ap.password), "%s", p_wifi_settings->ap_pwd);
    return ap_config;
}

static void
wifi_manager_esp_wifi_configure(const struct wifi_settings_t *p_wifi_settings)
{
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_config_t ap_config = wifi_manager_generate_ap_config(p_wifi_settings);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, p_wifi_settings->ap_bandwidth));
    ESP_ERROR_CHECK(esp_wifi_set_ps(p_wifi_settings->sta_power_save));
}

static void
wifi_manager_tcpip_adapter_set_default_ip(void)
{
    tcpip_adapter_ip_info_t info = { 0 };
    inet_pton(AF_INET, DEFAULT_AP_IP, &info.ip); /* access point is on a static IP */
    inet_pton(AF_INET, DEFAULT_AP_GATEWAY, &info.gw);
    inet_pton(AF_INET, DEFAULT_AP_NETMASK, &info.netmask);
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP)); /* stop AP DHCP server */
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP)); /* start AP DHCP server */
}

static void
wifi_manager_tcpip_adapter_configure(const struct wifi_settings_t *p_wifi_settings)
{
    if (p_wifi_settings->sta_static_ip)
    {
        LOG_INFO(
            "Assigning static ip to STA interface. IP: %s , GW: %s , Mask: %s",
            ip4addr_ntoa(&p_wifi_settings->sta_static_ip_config.ip),
            ip4addr_ntoa(&p_wifi_settings->sta_static_ip_config.gw),
            ip4addr_ntoa(&p_wifi_settings->sta_static_ip_config.netmask));

        /* stop DHCP client*/
        ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));
        /* assign a static IP to the STA network interface */
        ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &p_wifi_settings->sta_static_ip_config));
    }
    else
    {
        /* start DHCP client if not started*/
        LOG_INFO("wifi_manager: Start DHCP client for STA interface. If not already running");
        tcpip_adapter_dhcp_status_t status = 0;
        ESP_ERROR_CHECK(tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &status));
        if (status != TCPIP_ADAPTER_DHCP_STARTED)
        {
            ESP_ERROR_CHECK(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA));
        }
    }
}

ATTR_NORETURN
static void
wifi_manager(const void *p_params)
{
    const WiFiAntConfig_t *p_wifi_ant_config = p_params;

    /* initialize the tcp stack */
    tcpip_adapter_init();

    /* event handler and event group for the wifi driver */
    g_wifi_manager_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler, NULL));

    /* default wifi config */
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_manager_set_ant_config(p_wifi_ant_config);
    /* SoftAP - Wifi Access Point configuration setup */
    wifi_manager_tcpip_adapter_set_default_ip();
    const wifi_settings_t *p_wifi_settings = wifi_sta_config_get_wifi_settings();
    wifi_manager_esp_wifi_configure(p_wifi_settings);

    /* STA - Wifi Station configuration setup */
    wifi_manager_tcpip_adapter_configure(p_wifi_settings);

    /* by default the mode is STA because wifi_manager will not start the access point unless it has to! */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* enqueue first event: load previous config */
    wifiman_msg_send_cmd_load_restore_sta();

    wifi_manager_main_loop();

    vTaskDelete(NULL);
}
