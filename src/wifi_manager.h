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

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The actual WiFi settings in use
 */
struct wifi_settings_t
{
    uint8_t                 ap_ssid[MAX_SSID_SIZE];
    uint8_t                 ap_pwd[MAX_PASSWORD_SIZE];
    uint8_t                 ap_channel;
    uint8_t                 ap_ssid_hidden;
    wifi_bandwidth_t        ap_bandwidth;
    bool                    sta_only;
    wifi_ps_type_t          sta_power_save;
    bool                    sta_static_ip;
    tcpip_adapter_ip_info_t sta_static_ip_config;
};

extern struct wifi_settings_t wifi_settings;

typedef struct
{
    wifi_ant_gpio_config_t wifiAntGpioConfig;
    wifi_ant_config_t      wifiAntConfig;
} WiFiAntConfig_t;

/**
 * Allocate heap memory for the wifi manager and start the wifi_manager RTOS task
 */
void
wifi_manager_start(const WiFiAntConfig_t *pWiFiAntConfig);

/**
 * Frees up all memory allocated by the wifi_manager and kill the task.
 */
void
wifi_manager_destroy();

/**
 * Filters the AP scan list to unique SSIDs
 */
void
filter_unique(wifi_ap_record_t *aplist, uint16_t *ap_num);

/**
 * Main task for the wifi_manager
 */
void
wifi_manager(void *pvParameters);

/**
 * @brief clears the current STA wifi config in flash ram storage.
 */
esp_err_t
wifi_manager_clear_sta_config(void);

/**
 * @brief saves the current STA wifi config to flash ram storage.
 */
esp_err_t
wifi_manager_save_sta_config();

/**
 * @brief fetch a previously STA wifi config in the flash ram storage.
 * @return true if a previously saved config was found, false otherwise.
 */
bool
wifi_manager_fetch_wifi_sta_config();

wifi_config_t *
wifi_manager_get_wifi_sta_config();

const char *
wifi_manager_get_wifi_sta_ssid(void);

void
wifi_manager_stop();

/**
 * @brief A standard wifi event handler as recommended by Espressif
 */
void
wifi_manager_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data);

/**
 * @brief requests a connection to an access point that will be process in the main task thread.
 */
void
wifi_manager_connect_async();

/**
 * @brief requests a wifi scan
 */
void
wifi_manager_scan_async();

/**
 * @brief requests to disconnect and forget about the access point.
 */
void
wifi_manager_disconnect_async();

/**
 * @brief Tries to get access to json buffer mutex.
 *
 * The HTTP server can try to access the json to serve clients while the wifi manager thread can try
 * to update it. These two tasks are synchronized through a mutex.
 *
 * The mutex is used by both the access point list json and the connection status json.\n
 * These two resources should technically have their own mutex but we lose some flexibility to save
 * on memory.
 *
 * This is a simple wrapper around freeRTOS function xSemaphoreTake.
 *
 * @param xTicksToWait The time in ticks to wait for the semaphore to become available.
 * @return true in success, false otherwise.
 */
bool
wifi_manager_lock_json_buffer(TickType_t xTicksToWait);

/**
 * @brief Releases the json buffer mutex.
 */
void
wifi_manager_unlock_json_buffer();

/**
 * @brief Start the mDNS service
 */
void
wifi_manager_initialise_mdns();

/**
 * @brief Register a callback to a custom function when specific event message_code happens.
 */
void
wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void *));

BaseType_t
wifi_manager_send_message(message_code_t code, void *param);
BaseType_t
wifi_manager_send_message_to_front(message_code_t code, void *param);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_H_INCLUDED */
