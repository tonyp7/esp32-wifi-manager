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

@file json_access_points.h
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis
@note This file was created by extracting and rewriting the functions from wifi_manager.c by @author TheSomeMan

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef ESP32_WIFI_MANAGER_JSON_ACCESS_POINTS_H
#define ESP32_WIFI_MANAGER_JSON_ACCESS_POINTS_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void
json_access_points_init(void);

void
json_access_points_deinit(void);

/**
 * @brief Clear the list of access points.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void
json_access_points_clear(void);

/**
 * @brief Generates the list of access points after a wifi scan.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
const char *
json_access_points_generate(const wifi_ap_record_t *p_access_points, const uint32_t num_access_points);

const char *
json_access_points_get(void);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_JSON_ACCESS_POINTS_H
