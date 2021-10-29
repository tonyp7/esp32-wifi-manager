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

@file json_network_info.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis
@note This file was created by extracting and rewriting the functions from wifi_manager.c by @author TheSomeMan

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
@see https://github.com/ruuvi/esp32-wifi-manager
*/

#ifndef ESP32_WIFI_MANAGER_JSON_NETWORK_INFO_H
#define ESP32_WIFI_MANAGER_JSON_NETWORK_INFO_H

#include <stdint.h>
#include <stdbool.h>
#include "wifi_manager_defs.h"
#include "os_wrapper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct json_network_info_t
{
    wifi_ssid_t          ssid;
    network_info_str_t   network_info;
    update_reason_code_e update_reason_code;
    bool                 is_ssid_null;
    char                 extra_info[JSON_NETWORK_EXTRA_INFO_SIZE];
} json_network_info_t;

typedef struct http_server_resp_status_json_t
{
    char buf[JSON_IP_INFO_SIZE];
} http_server_resp_status_json_t;

typedef void (*json_network_info_do_action_callback_t)(json_network_info_t *const p_info, void *const p_param);

/**
 * @brief Init json_network_info
 */
void
json_network_info_init(void);

/**
 * @brief Deinit json_network_info
 */
void
json_network_info_deinit(void);

/**
 * @brief Try to lock access to json_network_info and perform specified action.
 * @note If access could not be gained within the specified timeout,
 *          then the callback-function will be called with NULL as the first argument.
 * @param cb_func - a callback-function to call after granting access to json_network_info
 * @param p_param - pointer to be passed to the callback-function
 * @param ticks_to_wait - timeout waiting for data access to be granted (use OS_DELTA_TICKS_INFINITE for infinite).
 */
void
json_network_info_do_action_with_timeout(
    json_network_info_do_action_callback_t cb_func,
    void *const                            p_param,
    const os_delta_ticks_t                 ticks_to_wait);

/**
 * @brief Lock access to json_network_info and perform specified action.
 * @param cb_func - a callback-function to call after granting access to json_network_info
 * @param p_param - pointer to be passed to the callback-function
 */
void
json_network_info_do_action(json_network_info_do_action_callback_t cb_func, void *const p_param);

/**
 * @brief Generates the connection status json: ssid and IP addresses.
 */
void
json_network_info_generate(http_server_resp_status_json_t *const p_resp_status_json, const bool flag_access_from_lan);

/**
 * @brief Generates the connection status json: ssid and IP addresses.
 * json_network_info_do_action_with_timeout
 */
void
json_network_info_do_generate(json_network_info_t *const p_info, void *const p_param);

/**
 * @brief Generates the connection status json: ssid and IP addresses.
 */
void
json_network_info_do_generate_internal(
    const json_network_info_t *const      p_info,
    http_server_resp_status_json_t *const p_resp_status_json,
    const bool                            flag_access_from_lan);

/**
 * @brief Updates the connection status info: ssid and IP addresses.
 */
void
json_network_info_update(
    const wifi_ssid_t *        p_ssid,
    const network_info_str_t * p_network_info,
    const update_reason_code_e update_reason_code);

/**
 * @brief Set extra info.
 */
void
json_network_set_extra_info(const char *const p_extra);

/**
 * @brief Clears the connection status json.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void
json_network_info_clear(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_JSON_NETWORK_INFO_H
