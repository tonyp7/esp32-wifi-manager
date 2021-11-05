/**
 * @file http_server_handle_req.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_handle_req.h"
#include <assert.h>
#include <string.h>
#include "wifi_manager.h"
#include "wifi_manager_internal.h"
#include "wifiman_msg.h"
#include "wifi_sta_config.h"
#include "json_access_points.h"
#include "json_network_info.h"
#include "http_server.h"
#include "http_server_auth.h"
#include "http_server_handle_req_get_auth.h"
#include "http_server_handle_req_post_auth.h"
#include "http_server_handle_req_delete_auth.h"

// Warning: Debug log level prints out the passwords as a "plaintext" so accidents won't happen.
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "http_server";

typedef struct http_server_gen_resp_status_json_param_t
{
    http_server_resp_t *const p_http_resp;
    bool                      flag_access_from_lan;
} http_server_gen_resp_status_json_param_t;

static http_server_resp_status_json_t g_resp_status_json;

static void
http_server_gen_resp_status_json(json_network_info_t *const p_info, void *const p_param)
{
    http_server_gen_resp_status_json_param_t *p_params = p_param;
    if (NULL == p_info)
    {
        LOG_ERR("http_server_netconn_serve: GET /status.json failed to obtain mutex");
        LOG_INFO("status.json: 503");
        *p_params->p_http_resp = http_server_resp_503();
    }
    else
    {
        json_network_info_do_generate_internal(p_info, &g_resp_status_json, p_params->flag_access_from_lan);
        LOG_INFO("status.json: %s", g_resp_status_json.buf);
        *p_params->p_http_resp = http_server_resp_200_json(g_resp_status_json.buf);
    }
}

static http_server_resp_t
http_server_handle_req_get(
    const char *const                    p_file_name_unchecked,
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    LOG_INFO("http_server_handle_req_get /%s", p_file_name_unchecked);

    const char *const p_file_name = (0 == strcmp(p_file_name_unchecked, "")) ? "index.html" : p_file_name_unchecked;

    const char *const p_file_ext = strrchr(p_file_name, '.');

    const wifi_ssid_t ap_ssid = wifi_sta_config_get_ap_ssid();

    const char *p_auth_q = "auth?";
    if ((0 == strcmp(p_file_name, "auth")) || (0 == strncmp(p_file_name, p_auth_q, strlen(p_auth_q))))
    {
        const http_server_resp_t resp_auth = http_server_handle_req_get_auth(
            flag_access_from_lan,
            http_header,
            p_remote_ip,
            p_auth_info,
            &ap_ssid,
            p_extra_header_fields);
        return resp_auth;
    }

    const http_server_resp_t resp_auth_check = http_server_handle_req_check_auth(
        flag_access_from_lan,
        http_header,
        p_remote_ip,
        p_auth_info,
        &ap_ssid,
        p_extra_header_fields);

    if ((NULL != p_file_ext) && ((0 == strcmp(p_file_ext, ".html")) || (0 == strcmp(p_file_ext, ".json"))))
    {
        if ((HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code) && (0 != strcmp(p_file_name, "auth.html")))
        {
            if ((HTTP_SERVER_AUTH_TYPE_RUUVI == p_auth_info->auth_type)
                || ((HTTP_SERVER_AUTH_TYPE_DENY == p_auth_info->auth_type)))
            {
                snprintf(
                    p_extra_header_fields->buf,
                    sizeof(p_extra_header_fields->buf),
                    "Set-Cookie: %s=/%s",
                    HTTP_SERVER_AUTH_RUUVI_COOKIE_PREV_URL,
                    p_file_name);
                return http_server_resp_302();
            }
            else
            {
                return resp_auth_check;
            }
        }

        if (0 == strcmp(p_file_name, "ap.json"))
        {
            const char *const p_buff = wifi_manager_scan_sync();
            if (NULL == p_buff)
            {
                LOG_ERR("GET /ap.json: failed to get json, return HTTP error 503");
                return http_server_resp_503();
            }
            LOG_INFO("ap.json: %s", p_buff);
            const http_server_resp_t resp = http_server_resp_200_json(p_buff);
            return resp;
        }
        else if (0 == strcmp(p_file_name, "status.json"))
        {
            http_server_update_last_http_status_request();

            http_server_resp_t                       http_resp = { 0 };
            http_server_gen_resp_status_json_param_t params    = {
                .p_http_resp          = &http_resp,
                .flag_access_from_lan = flag_access_from_lan,
            };
            const os_delta_ticks_t ticks_to_wait = 10U;
            json_network_info_do_action_with_timeout(&http_server_gen_resp_status_json, &params, ticks_to_wait);
            return http_resp;
        }
    }
    if ((NULL == p_file_ext) && (HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code))
    {
        if (HTTP_SERVER_AUTH_TYPE_RUUVI == p_auth_info->auth_type)
        {
            snprintf(
                p_extra_header_fields->buf,
                sizeof(p_extra_header_fields->buf),
                "Set-Cookie: %s=/%s",
                HTTP_SERVER_AUTH_RUUVI_COOKIE_PREV_URL,
                p_file_name);
            return http_server_resp_302();
        }
        else
        {
            return resp_auth_check;
        }
    }

    if (0 == strcmp(p_file_name, "auth.html"))
    {
        return wifi_manager_cb_on_http_get(p_file_name, flag_access_from_lan, &resp_auth_check);
    }
    else
    {
        return wifi_manager_cb_on_http_get(p_file_name, flag_access_from_lan, NULL);
    }
}

static http_server_resp_t
http_server_handle_req_delete(
    const char *                         p_file_name,
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    LOG_INFO("DELETE /%s", p_file_name);
    const wifi_ssid_t ap_ssid = wifi_sta_config_get_ap_ssid();

    const http_server_resp_t resp_auth_check = http_server_handle_req_check_auth(
        flag_access_from_lan,
        http_header,
        p_remote_ip,
        p_auth_info,
        &ap_ssid,
        p_extra_header_fields);

    if (HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code)
    {
        return http_server_resp_302();
    }

    if (0 == strcmp(p_file_name, "auth"))
    {
        return http_server_handle_req_delete_auth(
            http_header,
            p_remote_ip,
            p_auth_info,
            &ap_ssid,
            p_extra_header_fields);
    }
    else if (0 == strcmp(p_file_name, "connect.json"))
    {
        LOG_INFO("http_server_netconn_serve: DELETE /connect.json");
        if (wifi_manager_is_connected_to_ethernet())
        {
            wifi_manager_disconnect_eth();
        }
        else
        {
            /* request a disconnection from wifi and forget about it */
            wifi_manager_disconnect_wifi();
        }
        return http_server_resp_200_json("{}");
    }
    return wifi_manager_cb_on_http_delete(p_file_name, flag_access_from_lan, NULL);
}

static http_server_resp_t
http_server_handle_req_post_connect_json(const http_req_header_t http_header)
{
    LOG_INFO("http_server_netconn_serve: POST /connect.json");
    uint32_t    len_ssid     = 0;
    uint32_t    len_password = 0;
    const char *p_ssid       = http_req_header_get_field(http_header, "X-Custom-ssid:", &len_ssid);
    const char *p_password   = http_req_header_get_field(http_header, "X-Custom-pwd:", &len_password);
    if ((NULL == p_ssid) && (NULL == p_password))
    {
        LOG_INFO("POST /connect.json: SSID:NULL, PWD: NULL - connect to Ethernet");
        wifiman_msg_send_cmd_connect_eth();
        return http_server_resp_200_json("{}");
    }
    if ((NULL != p_ssid) && (len_ssid <= MAX_SSID_SIZE))
    {
        if (NULL == p_password)
        {
            const wifi_ssid_t saved_ssid = wifi_sta_config_get_ssid();
            if ((0 == strncmp(saved_ssid.ssid_buf, p_ssid, len_ssid)) && (strlen(saved_ssid.ssid_buf) == len_ssid))
            {
                LOG_INFO(
                    "POST /connect.json: SSID:%.*s, PWD: NULL - reconnect to saved WiFi",
                    (printf_int_t)len_ssid,
                    p_ssid);
            }
            else
            {
                LOG_WARN(
                    "POST /connect.json: SSID:%.*s, PWD: NULL - try to reconnect to saved WiFi, but previous SSID=%s",
                    (printf_int_t)len_ssid,
                    p_ssid,
                    saved_ssid.ssid_buf);
                wifi_sta_config_set_ssid_and_password(p_ssid, len_ssid, "", 0);
            }
            LOG_DBG("http_server_netconn_serve: wifi_manager_connect_async() call");
            wifi_manager_connect_async();
            return http_server_resp_200_json("{}");
        }
        else
        {
            LOG_DBG(
                "POST /connect.json: SSID:%.*s, PWD: %.*s - connect to WiFi",
                (printf_int_t)len_ssid,
                p_ssid,
                (printf_int_t)len_password,
                p_password);
            LOG_INFO("POST /connect.json: SSID:%.*s, PWD: ******** - connect to WiFi", (printf_int_t)len_ssid, p_ssid);
            if ((len_password <= MAX_PASSWORD_SIZE))
            {
                wifi_sta_config_set_ssid_and_password(p_ssid, len_ssid, p_password, len_password);

                LOG_DBG("http_server_netconn_serve: wifi_manager_connect_async() call");
                wifi_manager_connect_async();
                return http_server_resp_200_json("{}");
            }
        }
    }
    /* bad request the authentication header is not complete/not the correct format */
    return http_server_resp_400();
}

static http_server_resp_t
http_server_handle_req_post(
    const char *                         p_file_name,
    const bool                           flag_access_from_lan,
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    const http_req_body_t                http_body,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    LOG_INFO("POST /%s", p_file_name);

    const wifi_ssid_t ap_ssid = wifi_sta_config_get_ap_ssid();

    if (0 == strcmp(p_file_name, "auth"))
    {
        return http_server_handle_req_post_auth(
            flag_access_from_lan,
            http_header,
            p_remote_ip,
            http_body,
            p_auth_info,
            &ap_ssid,
            p_extra_header_fields);
    }

    const http_server_resp_t resp_auth_check = http_server_handle_req_check_auth(
        flag_access_from_lan,
        http_header,
        p_remote_ip,
        p_auth_info,
        &ap_ssid,
        p_extra_header_fields);

    if (HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code)
    {
        return http_server_resp_302();
    }

    if (0 == strcmp(p_file_name, "connect.json"))
    {
        return http_server_handle_req_post_connect_json(http_header);
    }
    return wifi_manager_cb_on_http_post(p_file_name, http_body, flag_access_from_lan);
}

http_server_resp_t
http_server_handle_req(
    const http_req_info_t *const         p_req_info,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    http_header_extra_fields_t *const    p_extra_header_fields,
    const bool                           flag_access_from_lan)
{
    assert(NULL != p_extra_header_fields);
    p_extra_header_fields->buf[0] = '\0';

    const char *path = p_req_info->http_uri.ptr;
    if ('/' == path[0])
    {
        path += 1;
    }

    if (0 == strcmp("GET", p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_get(
            path,
            flag_access_from_lan,
            p_req_info->http_header,
            p_remote_ip,
            p_auth_info,
            p_extra_header_fields);
    }
    else if (0 == strcmp("DELETE", p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_delete(
            path,
            flag_access_from_lan,
            p_req_info->http_header,
            p_remote_ip,
            p_auth_info,
            p_extra_header_fields);
    }
    else if (0 == strcmp("POST", p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_post(
            path,
            flag_access_from_lan,
            p_req_info->http_header,
            p_remote_ip,
            p_auth_info,
            p_req_info->http_body,
            p_extra_header_fields);
    }
    else
    {
        return http_server_resp_400();
    }
}
