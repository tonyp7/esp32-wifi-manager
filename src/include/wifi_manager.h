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

@file wifi_manager.h
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef WIFI_MANAGER_H_INCLUDED
#define WIFI_MANAGER_H_INCLUDED

#include "esp_wifi.h"
#include "wifi_manager_defs.h"
#include "http_server_resp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*wifi_manager_cb_ptr)(void *);

typedef struct
{
    wifi_ant_gpio_config_t wifi_ant_gpio_config;
    wifi_ant_config_t      wifi_ant_config;
} WiFiAntConfig_t;

typedef struct wifi_manager_callbacks_t
{
    wifi_manager_http_cb_on_user_req_t             cb_on_http_user_req;
    wifi_manager_http_callback_t                   cb_on_http_get;
    wifi_manager_http_cb_on_post_t                 cb_on_http_post;
    wifi_manager_http_callback_t                   cb_on_http_delete;
    wifi_manager_callback_on_cmd_connect_eth_t     cb_on_connect_eth_cmd;
    wifi_manager_callback_on_cmd_disconnect_eth_t  cb_on_disconnect_eth_cmd;
    wifi_manager_callback_on_cmd_disconnect_sta_t  cb_on_disconnect_sta_cmd;
    wifi_manager_callback_on_ap_sta_connected_t    cb_on_ap_sta_connected;
    wifi_manager_callback_on_ap_sta_disconnected_t cb_on_ap_sta_disconnected;
} wifi_manager_callbacks_t;

/**
 * @brief Allocate heap memory for the wifi manager and start the wifi_manager RTOS task.
 */
bool
wifi_manager_start(
    const bool                            flag_start_wifi,
    const bool                            flag_start_ap_only,
    const wifi_ssid_t *const              p_gw_wifi_ssid,
    const WiFiAntConfig_t *               p_wifi_ant_config,
    const wifi_manager_callbacks_t *const p_callbacks);

/**
 * @brief Stop WiFi access-point
 */
void
wifi_manager_stop_ap(void);

/**
 * @brief Start WiFi access-point
 */
void
wifi_manager_start_ap(void);

/**
 * @brief Checks if the current STA wifi config in NVS is valid.
 */
bool
wifi_manager_check_sta_config(void);

/**
 * @brief Clears the current STA wifi config in NVS.
 */
bool
wifi_manager_clear_sta_config(const wifi_ssid_t *const p_gw_wifi_ssid);

/**
 * @brief requests a connection to an access point that will be process in the main task thread.
 */
void
wifi_manager_connect_async(void);

/**
 * @brief requests a wifi scan
 */
void
wifi_manager_scan_async(void);

/**
 * @brief scan WiFi APs and return json
 */
const char *
wifi_manager_scan_sync(void);

/**
 * @brief requests to disconnect from Ethernet.
 */
void
wifi_manager_disconnect_eth(void);

/**
 * @brief requests to disconnect from the WiFi access point.
 */
void
wifi_manager_disconnect_wifi(void);

/**
 * @brief Register a callback to a custom function when specific event message_code happens.
 */
void
wifi_manager_set_callback(const message_code_e message_code, wifi_manager_cb_ptr func_ptr);

bool
wifi_manager_is_working(void);

bool
wifi_manager_is_ap_active(void);

bool
wifi_manager_is_connected_to_wifi(void);

bool
wifi_manager_is_connected_to_ethernet(void);

bool
wifi_manager_is_connected_to_wifi_or_ethernet(void);

bool
wifi_manager_is_ap_sta_ip_assigned(void);

bool
wifi_manager_is_sta_configured(void);

/**
 * @brief Generates the connection status json: ssid and IP addresses.
 * @param update_reason_code - connection status, see update_reason_code_e
 * @param p_ssid - pointer to wifi_ssid_t (WiFi SSID)
 * @param p_ip_info - pointer to tcpip_adapter_ip_info_t
 * @param p_dhcp_ip - pointer to ip4_addr_t with the DHCP server IP address or NULL
 */
void
wifi_manager_update_network_connection_info(
    const update_reason_code_e           update_reason_code,
    const wifi_ssid_t *const             p_ssid,
    const tcpip_adapter_ip_info_t *const p_ip_info,
    const ip4_addr_t *const              p_dhcp_ip);

void
wifi_manager_set_extra_info_for_status_json(const char *const p_extra);

const char *
wifi_manager_generate_json_access_points(void);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H_INCLUDED */
