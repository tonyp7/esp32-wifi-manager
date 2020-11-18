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

@file json_access_points.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis
@note This file was created by extracting and rewriting the functions from wifi_manager.c by @author TheSomeMan

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "json_access_points.h"
#include <stdint.h>
#include <stdio.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "json.h"

static char g_json_access_points_buf[JSON_ACCESS_POINT_BUF_SIZE];

esp_err_t
json_access_points_init(void)
{
    json_access_points_clear();
    return ESP_OK;
}

void
json_access_points_deinit(void)
{
    /* This is a public API of json_access_points.c
     * It's empty because internal buffer g_json_access_points_buf is statically allocated
     * and it does not require deinit actions.
     * */
}

void
json_access_points_clear(void)
{
    snprintf(g_json_access_points_buf, sizeof(g_json_access_points_buf), "[]\n");
}

void
json_access_points_generate(const wifi_ap_record_t *p_access_points, const uint32_t num_access_points)
{
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(g_json_access_points_buf);
    str_buf_printf(&str_buf, "[");
    const uint32_t num_ap_checked = (num_access_points <= MAX_AP_NUM) ? num_access_points : MAX_AP_NUM;
    for (uint32_t i = 0; i < num_ap_checked; i++)
    {
        const wifi_ap_record_t ap = p_access_points[i];

        str_buf_printf(&str_buf, "{\"ssid\":");
        json_print_escaped_string(&str_buf, (const char *)ap.ssid);

        /* print the rest of the json for this access point: no more string to escape */
        str_buf_printf(
            &str_buf,
            ",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%s\n",
            ap.primary,
            ap.rssi,
            ap.authmode,
            ((i) < (num_ap_checked - 1)) ? "," : "");
    }
    str_buf_printf(&str_buf, "]\n");
}

const char *
json_access_points_get(void)
{
    return g_json_access_points_buf;
}
