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
*/

#include "json_network_info.h"
#include <stddef.h>
#include "json.h"
#include "wifi_manager_defs.h"
#include "esp_type_wrapper.h"

static char g_json_network_info_buf[JSON_IP_INFO_SIZE];

void
json_network_info_init(void)
{
    json_network_info_clear();
}

void
json_network_info_deinit(void)
{
    /* This is a public API of json_network_info.c
     * It's empty because internal buffer g_json_network_info_buf is statically allocated
     * and it does not require deinit actions.
     * */
}

const char *
json_network_info_get(void)
{
    return g_json_network_info_buf;
}

void
json_network_info_generate(
    const wifi_ssid_t *        p_ssid,
    const network_info_str_t * p_network_info,
    const update_reason_code_e update_reason_code)
{
    if (NULL == p_ssid)
    {
        json_network_info_clear();
        return;
    }
    if ('\0' == p_ssid->ssid_buf[0])
    {
        json_network_info_clear();
        return;
    }
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(g_json_network_info_buf);
    str_buf_printf(&str_buf, "{\"ssid\":");
    json_print_escaped_string(&str_buf, p_ssid->ssid_buf);

    str_buf_printf(
        &str_buf,
        ",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"urc\":%d}\n",
        p_network_info->ip,
        p_network_info->netmask,
        p_network_info->gw,
        (printf_int_t)update_reason_code);
}

void
json_network_info_clear(void)
{
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(g_json_network_info_buf);
    str_buf_printf(&str_buf, "{}\n");
}
