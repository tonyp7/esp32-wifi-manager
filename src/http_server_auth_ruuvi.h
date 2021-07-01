/**
 * @file http_server_auth_ruuvi.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_RUUVI_H
#define ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_RUUVI_H

#include "http_server_auth_common.h"
#include "wifiman_sha256.h"
#include "http_req.h"
#include "sta_ip.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_AUTH_RUUVI_SESSION_ID_SIZE (16U + 1U)

#define HTTP_SERVER_AUTH_RUUVI_PREV_URL_SIZE (64U + 1U)

#define HTTP_SERVER_AUTH_RUUVI_COOKIE_SESSION  "RUUVISESSION"
#define HTTP_SERVER_AUTH_RUUVI_COOKIE_PREV_URL "RUUVI_PREV_URL"

#define HTTP_SERVER_AUTH_RUUVI_MAX_NUM_SESSIONS (4U)

typedef struct http_server_auth_ruuvi_req_t
{
    char username[HTTP_SERVER_MAX_AUTH_USER_LEN];
    char password[HTTP_SERVER_MAX_AUTH_PASS_LEN];
} http_server_auth_ruuvi_req_t;

typedef struct http_server_auth_ruuvi_session_id_t
{
    char buf[HTTP_SERVER_AUTH_RUUVI_SESSION_ID_SIZE];
} http_server_auth_ruuvi_session_id_t;

typedef struct http_server_auth_ruuvi_prev_url_t
{
    char buf[HTTP_SERVER_AUTH_RUUVI_PREV_URL_SIZE];
} http_server_auth_ruuvi_prev_url_t;

typedef struct http_server_auth_ruuvi_login_session_t
{
    wifiman_sha256_digest_hex_str_t     challenge;
    http_server_auth_ruuvi_session_id_t session_id;
    sta_ip_string_t                     remote_ip;
} http_server_auth_ruuvi_login_session_t;

typedef struct http_server_auth_ruuvi_authorized_session_t
{
    http_server_auth_ruuvi_session_id_t session_id;
    sta_ip_string_t                     remote_ip;
} http_server_auth_ruuvi_authorized_session_t;

typedef struct http_server_auth_ruuvi_t
{
    http_server_auth_ruuvi_login_session_t      login_session;
    http_server_auth_ruuvi_authorized_session_t authorized_sessions[HTTP_SERVER_AUTH_RUUVI_MAX_NUM_SESSIONS];
} http_server_auth_ruuvi_t;

bool
http_server_auth_ruuvi_get_session_id_from_cookies(
    const http_req_header_t                    http_header,
    http_server_auth_ruuvi_session_id_t *const p_session_id);

http_server_auth_ruuvi_prev_url_t
http_server_auth_ruuvi_get_prev_url_from_cookies(const http_req_header_t http_header);

http_server_auth_ruuvi_authorized_session_t *
http_server_auth_ruuvi_find_authorized_session(
    const http_server_auth_ruuvi_session_id_t *const p_session_id,
    const sta_ip_string_t *const                     p_remote_ip);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_RUUVI_H
