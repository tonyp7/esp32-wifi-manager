/**
 * @file http_server_handle_req.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_handle_req.h"
#include <assert.h>
#include <string.h>
#include "str_buf.h"
#include "os_malloc.h"
#include "cJSON.h"
#include "mbedtls/sha256.h"
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
#include "http_server_ecdh.h"

// Warning: Debug log level prints out the passwords as a "plaintext" so accidents won't happen.
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "http_server";

typedef struct http_server_gen_resp_status_json_param_t
{
    http_server_resp_t *const p_http_resp;
    bool                      flag_access_from_lan;
} http_server_gen_resp_status_json_param_t;

typedef struct wifi_ssid_password_t
{
    bool is_ssid_null;
    bool is_password_null;
    char ssid[MAX_SSID_SIZE];
    char password[MAX_PASSWORD_SIZE];
} wifi_ssid_password_t;

static http_server_resp_status_json_t g_resp_status_json;

static void
http_server_gen_resp_status_json(const json_network_info_t *const p_info, void *const p_param)
{
    http_server_gen_resp_status_json_param_t *p_params = p_param;
    if (NULL == p_info)
    {
        LOG_ERR("http_server_netconn_serve: GET /status.json failed to obtain mutex");
        LOG_DBG("status.json: 503");
        *p_params->p_http_resp = http_server_resp_503();
    }
    else
    {
        json_network_info_do_generate_internal(p_info, &g_resp_status_json, p_params->flag_access_from_lan);
        LOG_DBG("status.json: %s", g_resp_status_json.buf);
        *p_params->p_http_resp = http_server_resp_200_json(g_resp_status_json.buf);
    }
}

static http_server_resp_t
http_server_handle_req_get_html_or_json(
    const bool                           flag_access_from_lan,
    const char *const                    p_file_name,
    const http_server_auth_info_t *const p_auth_info,
    const http_server_resp_t *const      p_resp_auth_check,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    if ((HTTP_RESP_CODE_200 != p_resp_auth_check->http_resp_code) && (0 != strcmp(p_file_name, "auth.html")))
    {
        if ((HTTP_SERVER_AUTH_TYPE_RUUVI == p_auth_info->auth_type)
            || (HTTP_SERVER_AUTH_TYPE_DENY == p_auth_info->auth_type))
        {
            snprintf(
                p_extra_header_fields->buf,
                sizeof(p_extra_header_fields->buf),
                "Set-Cookie: %s=/%s",
                HTTP_SERVER_AUTH_RUUVI_COOKIE_PREV_URL,
                p_file_name);
            return http_server_resp_302();
        }
        return *p_resp_auth_check;
    }

    if (0 == strcmp(p_file_name, "ap.json"))
    {
        const char *const p_buff = wifi_manager_scan_sync();
        if (NULL == p_buff)
        {
            LOG_ERR("GET /ap.json: failed to get json, return HTTP error 503");
            return http_server_resp_503();
        }
        LOG_INFO("ap.json: %s", ((NULL != p_buff) ? p_buff : "NULL"));
        const http_server_resp_t resp = http_server_resp_200_json(p_buff);
        return resp;
    }
    if (0 == strcmp(p_file_name, "status.json"))
    {
        http_server_update_last_http_status_request();

        http_server_resp_t                       http_resp = { 0 };
        http_server_gen_resp_status_json_param_t params    = {
            .p_http_resp          = &http_resp,
            .flag_access_from_lan = flag_access_from_lan,
        };
        const os_delta_ticks_t ticks_to_wait = 10U;
        json_network_info_do_const_action_with_timeout(&http_server_gen_resp_status_json, &params, ticks_to_wait);
        return http_resp;
    }
    if (0 == strcmp(p_file_name, "auth.html"))
    {
        return wifi_manager_cb_on_http_get(p_file_name, flag_access_from_lan, p_resp_auth_check);
    }
    return http_server_resp_404();
}

static http_server_resp_t
http_server_handle_req_get_path_without_extension_api_key_not_used(
    const char *const                    p_file_name,
    const http_server_auth_info_t *const p_auth_info,
    const http_server_resp_t *const      p_resp_auth_check,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    if (HTTP_RESP_CODE_200 != p_resp_auth_check->http_resp_code)
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
        return *p_resp_auth_check;
    }
    return http_server_resp_data_in_flash(HTTP_CONENT_TYPE_TEXT_PLAIN, NULL, 0, HTTP_CONENT_ENCODING_NONE, NULL);
}

static http_server_resp_t
http_server_handle_req_get_path_without_extension(
    const char *const                    p_file_name,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    const http_server_resp_t *const      p_resp_auth_check,
    const http_server_auth_api_key_e     access_by_api_key,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    switch (access_by_api_key)
    {
        case HTTP_SERVER_AUTH_API_KEY_NOT_USED:
            return http_server_handle_req_get_path_without_extension_api_key_not_used(
                p_file_name,
                p_auth_info,
                p_resp_auth_check,
                p_extra_header_fields);
        case HTTP_SERVER_AUTH_API_KEY_ALLOWED:
            break;
        case HTTP_SERVER_AUTH_API_KEY_PROHIBITED:
            return http_server_resp_401_json(http_server_fill_auth_json_bearer_failed(p_ap_ssid));
    }
    return http_server_resp_data_in_flash(HTTP_CONENT_TYPE_TEXT_PLAIN, NULL, 0, HTTP_CONENT_ENCODING_NONE, NULL);
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
    LOG_DBG("http_server_handle_req_get /%s", p_file_name_unchecked);

    const char *const p_file_name = (0 == strcmp(p_file_name_unchecked, "")) ? "index.html" : p_file_name_unchecked;

    const char *const p_file_ext = strrchr(p_file_name, '.');

    const wifi_ssid_t ap_ssid = wifi_sta_config_get_ap_ssid();

    if (0 == strcmp(p_file_name, "auth"))
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

    http_server_auth_api_key_e access_by_api_key = HTTP_SERVER_AUTH_API_KEY_NOT_USED;
    const http_server_resp_t   resp_auth_check   = http_server_handle_req_check_auth(
        flag_access_from_lan,
        http_header,
        p_remote_ip,
        p_auth_info,
        &ap_ssid,
        p_extra_header_fields,
        &access_by_api_key);

    if (NULL != p_file_ext)
    {
        if ((0 == strcmp(p_file_ext, ".html")) || (0 == strcmp(p_file_ext, ".json")))
        {
            const http_server_resp_t resp_auth_check_for_html_or_json = http_server_handle_req_get_html_or_json(
                flag_access_from_lan,
                p_file_name,
                p_auth_info,
                &resp_auth_check,
                p_extra_header_fields);
            if (HTTP_RESP_CODE_404 != resp_auth_check_for_html_or_json.http_resp_code)
            {
                return resp_auth_check_for_html_or_json;
            }
        }
    }
    else
    {
        const http_server_resp_t resp_auth_check_with_api_key = http_server_handle_req_get_path_without_extension(
            p_file_name,
            p_auth_info,
            &ap_ssid,
            &resp_auth_check,
            access_by_api_key,
            p_extra_header_fields);
        if (HTTP_RESP_CODE_200 != resp_auth_check_with_api_key.http_resp_code)
        {
            return resp_auth_check_with_api_key;
        }
    }
    return wifi_manager_cb_on_http_get(p_file_name, flag_access_from_lan, NULL);
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
        p_extra_header_fields,
        NULL);

    if (HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code)
    {
        return http_server_resp_302();
    }

    if (0 == strcmp(p_file_name, "auth"))
    {
        return http_server_handle_req_delete_auth(http_header, p_remote_ip, p_auth_info, &ap_ssid);
    }
    if (0 == strcmp(p_file_name, "connect.json"))
    {
        LOG_INFO("http_server_netconn_serve: DELETE /connect.json");
        if (wifi_manager_is_connected_to_ethernet())
        {
            wifi_manager_disconnect_eth();
        }
        else
        {
            /* request a disconnection from Wi-Fi and forget about it */
            wifi_manager_disconnect_wifi();
        }
        return http_server_resp_200_json("{}");
    }
    return wifi_manager_cb_on_http_delete(p_file_name, flag_access_from_lan, NULL);
}

static const char *
http_server_json_get_string_val_ptr(const cJSON *const p_json_root, const char *const p_attr_name)
{
    const cJSON *const p_json_attr = cJSON_GetObjectItem(p_json_root, p_attr_name);
    if (NULL == p_json_attr)
    {
        return NULL;
    }
    return cJSON_GetStringValue(p_json_attr);
}

static bool
http_server_handle_ruuvi_ecdh_pub_key(
    const http_req_header_t               http_header,
    http_server_ecdh_pub_key_b64_t *const p_pub_key_b64_srv)
{
    uint32_t          len_ruuvi_ecdh_pub_key = 0;
    const char *const p_ruuvi_ecdh_pub_key   = http_req_header_get_field(
        http_header,
        "ruuvi_ecdh_pub_key:",
        &len_ruuvi_ecdh_pub_key);

    if (NULL == p_ruuvi_ecdh_pub_key)
    {
        return false;
    }

    LOG_INFO("Found ruuvi_ecdh_pub_key: %.*s", (printf_int_t)len_ruuvi_ecdh_pub_key, p_ruuvi_ecdh_pub_key);

    http_server_ecdh_pub_key_b64_t pub_key_b64_cli = { 0 };
    if (len_ruuvi_ecdh_pub_key >= sizeof(pub_key_b64_cli.buf))
    {
        LOG_ERR(
            "Length of ecdh_pub_key_b64 (%u) is longer than expected (%u)",
            len_ruuvi_ecdh_pub_key,
            sizeof(pub_key_b64_cli.buf));
        return false;
    }

    snprintf(pub_key_b64_cli.buf, sizeof(pub_key_b64_cli.buf), "%.*s", len_ruuvi_ecdh_pub_key, p_ruuvi_ecdh_pub_key);
    if (!http_server_ecdh_handshake(&pub_key_b64_cli, p_pub_key_b64_srv))
    {
        LOG_ERR("%s failed", "http_server_ecdh_handshake");
        return false;
    }
    return true;
}

static bool
http_server_parse_encrypted_req(cJSON *p_json_root, http_server_ecdh_encrypted_req_t *const p_enc_req)
{
    p_enc_req->p_encrypted = http_server_json_get_string_val_ptr(p_json_root, "encrypted");
    if (NULL == p_enc_req->p_encrypted)
    {
        return false;
    }
    p_enc_req->p_iv = http_server_json_get_string_val_ptr(p_json_root, "iv");
    if (NULL == p_enc_req->p_iv)
    {
        return false;
    }
    p_enc_req->p_hash = http_server_json_get_string_val_ptr(p_json_root, "hash");
    if (NULL == p_enc_req->p_hash)
    {
        return false;
    }
    return true;
}

static bool
http_server_decrypt(const http_req_body_t http_body, str_buf_t *p_str_buf)
{
    cJSON *p_json_root = cJSON_Parse(http_body.ptr);
    if (NULL == p_json_root)
    {
        LOG_ERR("Failed to parse json or no memory");
        return false;
    }
    http_server_ecdh_encrypted_req_t enc_req = { 0 };
    if (!http_server_parse_encrypted_req(p_json_root, &enc_req))
    {
        LOG_ERR("Failed to parse encrypted request");
        cJSON_Delete(p_json_root);
        return false;
    }
    if (!http_server_ecdh_decrypt(&enc_req, p_str_buf))
    {
        LOG_ERR("Failed to decrypt request");
        cJSON_Delete(p_json_root);
        return false;
    }
    cJSON_Delete(p_json_root);
    LOG_DBG("Decrypted: %s", p_str_buf->buf);
    return true;
}

static bool
http_server_parse_cjson_wifi_ssid_password(const cJSON *const p_json_root, wifi_ssid_password_t *const p_info)
{
    const cJSON *const p_json_attr_ssid = cJSON_GetObjectItem(p_json_root, "ssid");
    if (NULL == p_json_attr_ssid)
    {
        LOG_ERR("connect.json: Can't find attribute 'ssid'");
        return false;
    }
    const char *       p_ssid               = cJSON_GetStringValue(p_json_attr_ssid);
    const cJSON *const p_json_attr_password = cJSON_GetObjectItem(p_json_root, "password");
    if (NULL == p_json_attr_password)
    {
        LOG_ERR("connect.json: Can't find attribute 'password'");
        return false;
    }
    const char *p_password = cJSON_GetStringValue(p_json_attr_password);

    if (NULL == p_ssid)
    {
        p_info->is_ssid_null = true;
        p_info->ssid[0]      = '\0';
    }
    else
    {
        p_info->is_ssid_null = false;
        if (strlen(p_ssid) >= sizeof(p_info->ssid))
        {
            LOG_ERR("connect.json: SSID is too long");
            return false;
        }
        snprintf(p_info->ssid, sizeof(p_info->ssid), "%s", p_ssid);
    }

    if (NULL == p_password)
    {
        p_info->is_password_null = true;
        p_info->password[0]      = '\0';
    }
    else
    {
        p_info->is_password_null = false;
        if (strlen(p_password) >= sizeof(p_info->password))
        {
            LOG_ERR("connect.json: password is too long");
            return false;
        }
        snprintf(p_info->password, sizeof(p_info->password), "%s", p_password);
    }
    return true;
}

static bool
http_server_parse_json_wifi_ssid_password(const char *const p_json, wifi_ssid_password_t *const p_info)
{
    cJSON *p_json_root = cJSON_Parse(p_json);
    if (NULL == p_json_root)
    {
        LOG_ERR("connect.json: Failed to parse decrypted content or no memory");
        return false;
    }
    const bool res = http_server_parse_cjson_wifi_ssid_password(p_json_root, p_info);
    cJSON_Delete(p_json_root);
    return res;
}

static http_server_resp_t
http_server_handle_req_post_connect_json(const http_req_body_t http_body)
{
    LOG_INFO("http_server_netconn_serve: POST /connect.json");
    str_buf_t decrypted_content = STR_BUF_INIT_NULL();
    if (!http_server_decrypt(http_body, &decrypted_content))
    {
        return http_server_resp_400();
    }
    LOG_INFO("http_server_netconn_serve: decrypted: %s", decrypted_content.buf);
    wifi_ssid_password_t login_info = { 0 };
    if (!http_server_parse_json_wifi_ssid_password(decrypted_content.buf, &login_info))
    {
        str_buf_free_buf(&decrypted_content);
        return http_server_resp_400();
    }

    if (login_info.is_ssid_null && login_info.is_password_null)
    {
        LOG_INFO("POST /connect.json: SSID:NULL, PWD: NULL - connect to Ethernet");
        wifiman_msg_send_cmd_connect_eth();
        return http_server_resp_200_json("{}");
    }
    if (!login_info.is_ssid_null)
    {
        if (login_info.is_password_null)
        {
            const wifi_ssid_t saved_ssid = wifi_sta_config_get_ssid();
            if (0 == strcmp(saved_ssid.ssid_buf, login_info.ssid))
            {
                LOG_INFO("POST /connect.json: SSID:%s, PWD: NULL - reconnect to saved WiFi", login_info.ssid);
            }
            else
            {
                LOG_WARN(
                    "POST /connect.json: SSID:%s, PWD: NULL - try to connect to WiFi without authentication",
                    login_info.ssid);
                wifi_sta_config_set_ssid_and_password(login_info.ssid, strlen(login_info.ssid), "", 0);
            }
            LOG_DBG("http_server_netconn_serve: wifi_manager_connect_async() call");
            wifi_manager_connect_async();
            return http_server_resp_200_json("{}");
        }
        LOG_DBG(
            "POST /connect.json: SSID:%.*s, PWD: %.*s - connect to WiFi",
            (printf_int_t)len_ssid,
            p_ssid,
            (printf_int_t)len_password,
            p_password);
        LOG_INFO("POST /connect.json: SSID:%s, PWD: ******** - connect to WiFi", login_info.ssid);
        wifi_sta_config_set_ssid_and_password(
            login_info.ssid,
            strlen(login_info.ssid),
            login_info.password,
            strlen(login_info.password));

        LOG_DBG("http_server_netconn_serve: wifi_manager_connect_async() call");
        wifi_manager_connect_async();
        return http_server_resp_200_json("{}");
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
        p_extra_header_fields,
        NULL);

    if (HTTP_RESP_CODE_200 != resp_auth_check.http_resp_code)
    {
        return http_server_resp_302();
    }

    if (0 == strcmp(p_file_name, "connect.json"))
    {
        return http_server_handle_req_post_connect_json(http_body);
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
        const http_server_resp_t resp = http_server_handle_req_get(
            path,
            flag_access_from_lan,
            p_req_info->http_header,
            p_remote_ip,
            p_auth_info,
            p_extra_header_fields);

        http_server_ecdh_pub_key_b64_t pub_key_b64_srv = { 0 };
        if (http_server_handle_ruuvi_ecdh_pub_key(p_req_info->http_header, &pub_key_b64_srv))
        {
            const size_t offset = strlen(p_extra_header_fields->buf);
            snprintf(
                &p_extra_header_fields->buf[offset],
                sizeof(p_extra_header_fields->buf) - offset,
                "ruuvi_ecdh_pub_key: %s\r\n",
                pub_key_b64_srv.buf);
        }
        return resp;
    }
    if (0 == strcmp("DELETE", p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_delete(
            path,
            flag_access_from_lan,
            p_req_info->http_header,
            p_remote_ip,
            p_auth_info,
            p_extra_header_fields);
    }
    if (0 == strcmp("POST", p_req_info->http_cmd.ptr))
    {
        bool              flag_encrypted           = false;
        uint32_t          len_ruuvi_ecdh_encrypted = 0;
        const char *const p_ruuvi_ecdh_encrypted   = http_req_header_get_field(
            p_req_info->http_header,
            "ruuvi_ecdh_encrypted:",
            &len_ruuvi_ecdh_encrypted);
        if (NULL != p_ruuvi_ecdh_encrypted)
        {
            if (0 == strncmp(p_ruuvi_ecdh_encrypted, "true", len_ruuvi_ecdh_encrypted))
            {
                flag_encrypted = true;
            }
        }

        str_buf_t       decrypted_str_buf   = STR_BUF_INIT_NULL();
        http_req_body_t http_body_decrypted = {
            .ptr = NULL,
        };
        if (flag_encrypted)
        {
            if (!http_server_decrypt(p_req_info->http_body, &decrypted_str_buf))
            {
                return http_server_resp_400();
            }
            http_body_decrypted.ptr = decrypted_str_buf.buf;
        }

        const http_server_resp_t resp = http_server_handle_req_post(
            path,
            flag_access_from_lan,
            p_req_info->http_header,
            p_remote_ip,
            p_auth_info,
            flag_encrypted ? http_body_decrypted : p_req_info->http_body,
            p_extra_header_fields);
        if (flag_encrypted)
        {
            str_buf_free_buf(&decrypted_str_buf);
        }
        return resp;
    }
    return http_server_resp_400();
}
