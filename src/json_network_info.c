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
#include "os_mutex.h"

static json_network_info_t g_json_network_info;
static os_mutex_t          g_json_network_mutex;
static os_mutex_static_t   g_json_network_mutex_mem;

static json_network_info_t *
json_network_info_lock_with_timeout(const os_delta_ticks_t ticks_to_wait)
{
    if (NULL == g_json_network_mutex)
    {
        g_json_network_mutex = os_mutex_create_static(&g_json_network_mutex_mem);
    }
    if (!os_mutex_lock_with_timeout(g_json_network_mutex, ticks_to_wait))
    {
        return NULL;
    }
    return &g_json_network_info;
}

static void
json_network_info_unlock(json_network_info_t **pp_info)
{
    if (NULL != *pp_info)
    {
        *pp_info = NULL;
        os_mutex_unlock(g_json_network_mutex);
    }
}

void
json_network_info_do_action_with_timeout(
    void (*cb_func)(json_network_info_t *const p_info, void *const p_param),
    void *const            p_param,
    const os_delta_ticks_t ticks_to_wait)
{
    json_network_info_t *p_info = json_network_info_lock_with_timeout(ticks_to_wait);
    cb_func(p_info, p_param);
    json_network_info_unlock(&p_info);
}

void
json_network_info_do_action(
    void (*cb_func)(json_network_info_t *const p_info, void *const p_param),
    void *const p_param)
{
    json_network_info_do_action_with_timeout(cb_func, p_param, OS_DELTA_TICKS_INFINITE);
}

void
json_network_info_init(void)
{
    json_network_info_clear();
}

void
json_network_info_deinit(void)
{
    /* This is a public API of json_network_info.c
     * It's empty because internal buffer g_json_network_info is statically allocated
     * and it does not require deinit actions.
     * */
}

static void
json_network_info_do_clear(json_network_info_t *const p_info, void *const p_param)
{
    str_buf_t str_buf = STR_BUF_INIT_WITH_ARR(p_info->json_buf);
    str_buf_printf(&str_buf, "{}\n");
}

void
json_network_info_clear(void)
{
    json_network_info_do_action(&json_network_info_do_clear, NULL);
}

typedef struct json_network_info_generate_t
{
    const wifi_ssid_t *const        p_ssid;
    const network_info_str_t *const p_network_info;
    const update_reason_code_e      update_reason_code;
} json_network_info_generate_t;

static void
json_network_info_do_generate(json_network_info_t *const p_info, void *const p_param)
{
    json_network_info_generate_t *p_gen_info = p_param;
    str_buf_t                     str_buf    = STR_BUF_INIT_WITH_ARR(p_info->json_buf);
    str_buf_printf(&str_buf, "{\"ssid\":");
    if (NULL != p_gen_info->p_ssid)
    {
        json_print_escaped_string(&str_buf, p_gen_info->p_ssid->ssid_buf);
    }
    else
    {
        str_buf_printf(&str_buf, "null");
    }

    str_buf_printf(
        &str_buf,
        ",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"urc\":%d}\n",
        p_gen_info->p_network_info->ip,
        p_gen_info->p_network_info->netmask,
        p_gen_info->p_network_info->gw,
        (printf_int_t)p_gen_info->update_reason_code);
}

void
json_network_info_generate(
    const wifi_ssid_t *const        p_ssid,
    const network_info_str_t *const p_network_info,
    const update_reason_code_e      update_reason_code)
{
    json_network_info_generate_t gen_info = {
        .p_ssid             = p_ssid,
        .p_network_info     = p_network_info,
        .update_reason_code = update_reason_code,
    };
    json_network_info_do_action(&json_network_info_do_generate, &gen_info);
}
