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
#include <stdbool.h>
#include "esp_system.h"
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "nvs.h"
#include "lwip/ip4_addr.h"
#include "esp_netif.h"
#include "json_network_info.h"
#include "json_access_points.h"
#include "sta_ip_safe.h"
#include "ap_ssid.h"
#include "wifiman_msg.h"
#include "wifi_sta_config.h"
#include "http_req.h"
#include "os_timer.h"
#include "http_server_ecdh.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifi_manager";

EventGroupHandle_t        g_p_wifi_manager_event_group;
static StaticEventGroup_t g_wifi_manager_event_group_mem;

static os_timer_periodic_cptr_without_arg_t *g_p_wifi_manager_timer_task_watchdog;
static os_timer_periodic_static_t            g_wifi_manager_timer_task_watchdog_mem;

void
wifi_manager_disconnect_eth(void)
{
    wifiman_msg_send_cmd_disconnect_eth();
}

void
wifi_manager_disconnect_wifi(void)
{
    wifiman_msg_send_cmd_disconnect_sta();
}
bool
wifi_manager_start(
    const bool                                 flag_start_wifi,
    const bool                                 flag_start_ap_only,
    const wifi_ssid_t *const                   p_gw_wifi_ssid,
    const wifi_manager_antenna_config_t *const p_wifi_ant_config,
    const wifi_manager_callbacks_t *const      p_callbacks,
    int (*f_rng)(void *, unsigned char *, size_t),
    void *p_rng)
{
    wifi_manager_init_mutex();
    wifi_manager_lock();

    if (NULL == g_p_wifi_manager_event_group)
    {
        // wifi_manager can be re-started after stopping,
        // this global variable is not released on stopping,
        // so, we need to initialize it only on the first start.
        g_p_wifi_manager_event_group = xEventGroupCreateStatic(&g_wifi_manager_event_group_mem);
    }

    if (!http_server_ecdh_init(f_rng, p_rng))
    {
        xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_IS_WORKING);
        wifi_manager_unlock();
        return false;
    }

    if (!wifi_manager_init(flag_start_wifi, flag_start_ap_only, p_gw_wifi_ssid, p_wifi_ant_config, p_callbacks))
    {
        xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_IS_WORKING);
        wifi_manager_unlock();
        return false;
    }

    wifi_manager_unlock();
    return true;
}

bool
wifi_manager_check_sta_config(void)
{
    return wifi_sta_config_check();
}

bool
wifi_manager_clear_sta_config(const wifi_ssid_t *const p_gw_wifi_ssid)
{
    wifi_sta_config_init(p_gw_wifi_ssid);
    return wifi_sta_config_clear();
}

void
wifi_manager_update_network_connection_info(
    const update_reason_code_e       update_reason_code,
    const wifi_ssid_t *const         p_ssid,
    const esp_netif_ip_info_t *const p_ip_info,
    const esp_ip4_addr_t *const      p_dhcp_ip)
{
    network_info_str_t ip_info_str = {
        .ip      = { "0" },
        .gw      = { "0" },
        .netmask = { "0" },
        .dhcp    = { "" },
    };
    if (UPDATE_CONNECTION_OK == update_reason_code)
    {
        if (NULL == p_ssid)
        {
            xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_ETH_CONNECTED_BIT);
        }
        else
        {
            xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
        }
        if (NULL != p_ip_info)
        {
            /* save IP as a string for the HTTP server host */
            sta_ip_safe_set(p_ip_info->ip.addr);

            esp_ip4addr_ntoa(&p_ip_info->ip, ip_info_str.ip, sizeof(ip_info_str.ip));
            esp_ip4addr_ntoa(&p_ip_info->netmask, ip_info_str.netmask, sizeof(ip_info_str.netmask));
            esp_ip4addr_ntoa(&p_ip_info->gw, ip_info_str.gw, sizeof(ip_info_str.gw));
            if (NULL != p_dhcp_ip)
            {
                esp_ip4addr_ntoa(p_dhcp_ip, ip_info_str.dhcp.buf, sizeof(ip_info_str.dhcp.buf));
                LOG_INFO("DHCP IP: %s", ip_info_str.dhcp.buf);
            }
        }
        else
        {
            sta_ip_safe_reset();
        }
    }
    else
    {
        sta_ip_safe_reset();
        xEventGroupClearBits(
            g_p_wifi_manager_event_group,
            WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_ETH_CONNECTED_BIT);
    }
    json_network_info_update(p_ssid, &ip_info_str, update_reason_code);
}

void
wifi_manager_connect_async(void)
{
    /* in order to avoid a false positive on the front end app we need to quickly flush the ip json
     * There'se a risk the front end sees an IP or a password error when in fact
     * it's a remnant from a previous connection
     */
    wifi_manager_lock();
    json_network_info_clear();
    wifi_manager_unlock();
    wifiman_msg_send_cmd_connect_sta(CONNECTION_REQUEST_USER);
}

void
wifi_manager_stop_ap(void)
{
    LOG_INFO("%s", __func__);
    wifiman_msg_send_cmd_stop_ap();
}

void
wifi_manager_start_ap(void)
{
    LOG_INFO("%s", __func__);
    wifiman_msg_send_cmd_start_ap();
}

static void
wifi_manager_timer_cb_task_watchdog_feed(const os_timer_periodic_cptr_without_arg_t *const p_timer)
{
    (void)p_timer;
    wifiman_msg_send_cmd_task_watchdog_feed();
}

static void
wifi_manager_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_periodic_cptr_without_arg_start(g_p_wifi_manager_timer_task_watchdog);
}

void
wifi_manager_task(void)
{
    LOG_INFO("TaskWatchdog: Create timer");
    g_p_wifi_manager_timer_task_watchdog = os_timer_periodic_cptr_without_arg_create_static(
        &g_wifi_manager_timer_task_watchdog_mem,
        "wifi:wdog",
        pdMS_TO_TICKS(CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000U / 3U),
        &wifi_manager_timer_cb_task_watchdog_feed);

    wifi_manager_wdt_add_and_start();

    for (;;)
    {
        if (wifi_manager_recv_and_handle_msg())
        {
            break;
        }
    }

    LOG_INFO("Finish task");
    wifi_manager_lock();

    LOG_INFO("TaskWatchdog: Unregister current thread");
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());
    LOG_INFO("TaskWatchdog: Stop timer");
    os_timer_periodic_cptr_without_arg_stop(g_p_wifi_manager_timer_task_watchdog);
    LOG_INFO("TaskWatchdog: Delete timer");
    os_timer_periodic_cptr_without_arg_delete(&g_p_wifi_manager_timer_task_watchdog);

    // Do not delete gh_wifi_json_mutex
    // Do not delete g_p_wifi_manager_event_group
    xEventGroupClearBits(
        g_p_wifi_manager_event_group,
        WIFI_MANAGER_IS_WORKING | WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_AP_STA_CONNECTED_BIT
            | WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT | WIFI_MANAGER_AP_STARTED_BIT | WIFI_MANAGER_REQUEST_STA_CONNECT_BIT
            | WIFI_MANAGER_REQUEST_RESTORE_STA_BIT | WIFI_MANAGER_SCAN_BIT | WIFI_MANAGER_REQUEST_DISCONNECT_BIT
            | WIFI_MANAGER_AP_ACTIVE);

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_manager_event_handler);

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    /* heap buffers */
    json_access_points_deinit();
    json_network_info_deinit();
    sta_ip_safe_deinit();

    wifiman_msg_deinit();

    esp_netif_t *const p_netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_action_stop(p_netif_sta, NULL, 0, NULL);

    esp_netif_t *const p_netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_action_stop(p_netif_ap, NULL, 0, NULL);

    wifi_manager_unlock();
}

bool
wifi_manager_is_working(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_IS_WORKING));
}

bool
wifi_manager_is_ap_active(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_AP_ACTIVE));
}

bool
wifi_manager_is_connected_to_wifi(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_WIFI_CONNECTED_BIT));
}

bool
wifi_manager_is_connected_to_ethernet(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_ETH_CONNECTED_BIT));
}

bool
wifi_manager_is_connected_to_wifi_or_ethernet(void)
{
    const uint32_t event_group_bit_mask = WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_ETH_CONNECTED_BIT;
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & event_group_bit_mask));
}

bool
wifi_manager_is_ap_sta_ip_assigned(void)
{
    return (0 != (xEventGroupGetBits(g_p_wifi_manager_event_group) & WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT));
}

bool
wifi_manager_is_sta_configured(void)
{
    return wifi_sta_config_is_ssid_configured();
}

void
wifi_manager_set_extra_info_for_status_json(const char *const p_extra)
{
    json_network_set_extra_info(p_extra);
}
