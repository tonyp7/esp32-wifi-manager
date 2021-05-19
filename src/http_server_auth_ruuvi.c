/**
 * @file http_server_auth_ruuvi.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_auth_ruuvi.h"
#include <string.h>
#include "http_server_auth.h"
#include "http_req.h"

static bool
http_server_auth_ruuvi_get_ruuvi_session_from_cookies(
    const char *const                          p_cookies,
    const uint32_t                             len_cookie,
    http_server_auth_ruuvi_session_id_t *const p_session_id)
{
    p_session_id->buf[0]              = '\0';
    const char *const p_ruuvi_session = http_server_strnstr(
        p_cookies,
        HTTP_SERVER_AUTH_RUUVI_COOKIE_SESSION,
        len_cookie);
    if (NULL == p_ruuvi_session)
    {
        return false;
    }
    const size_t len_from_ruuvi_session_till_eol = len_cookie - (ptrdiff_t)(p_ruuvi_session - p_cookies);
    const char * p_end_of_cookie = http_server_strnstr(p_ruuvi_session, ";", len_from_ruuvi_session_till_eol);
    if (NULL == p_end_of_cookie)
    {
        p_end_of_cookie = p_ruuvi_session + len_from_ruuvi_session_till_eol;
    }
    const size_t      len_ruuvi_session_cookie = p_end_of_cookie - p_ruuvi_session;
    const char *const p_delimiter              = http_server_strnstr(p_ruuvi_session, "=", len_ruuvi_session_cookie);
    if ((NULL == p_delimiter) || (p_delimiter > p_end_of_cookie))
    {
        return false;
    }
    const char *const p_cookie_value = p_delimiter + 1;
    const size_t      cookie_len     = p_end_of_cookie - p_cookie_value;
    if (0 == cookie_len)
    {
        return false;
    }
    if (cookie_len >= sizeof(p_session_id->buf))
    {
        return false;
    }
    strncpy(p_session_id->buf, p_cookie_value, cookie_len);
    p_session_id->buf[cookie_len] = '\0';
    return true;
}

http_server_auth_ruuvi_authorized_session_t *
http_server_auth_ruuvi_find_authorized_session(
    const http_server_auth_ruuvi_session_id_t *const p_session_id,
    const sta_ip_string_t *const                     p_remote_ip)
{
    http_server_auth_ruuvi_t *const p_auth_ruuvi = http_server_auth_ruuvi_get_info();

    for (uint32_t i = 0; i < HTTP_SERVER_AUTH_RUUVI_MAX_NUM_SESSIONS; ++i)
    {
        http_server_auth_ruuvi_authorized_session_t *const p_auth_session = &p_auth_ruuvi->authorized_sessions[i];
        if ('\0' == p_auth_session->session_id.buf[0])
        {
            continue;
        }
        if ((0 == strcmp(p_auth_session->session_id.buf, p_session_id->buf))
            && (sta_ip_cmp(&p_auth_session->remote_ip, p_remote_ip)))
        {
            return p_auth_session;
        }
    }
    return NULL;
}

bool
http_server_auth_ruuvi_get_session_id_from_cookies(
    const http_req_header_t                    http_header,
    http_server_auth_ruuvi_session_id_t *const p_session_id)
{
    p_session_id->buf[0]         = '\0';
    uint32_t          len_cookie = 0;
    const char *const p_cookies  = http_req_header_get_field(http_header, "Cookie:", &len_cookie);
    if (NULL == p_cookies)
    {
        return false;
    }
    if (!http_server_auth_ruuvi_get_ruuvi_session_from_cookies(p_cookies, len_cookie, p_session_id))
    {
        return false;
    }
    return true;
}