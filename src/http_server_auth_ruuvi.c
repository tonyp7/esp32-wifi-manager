/**
 * @file http_server_auth_ruuvi.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_auth_ruuvi.h"
#include <string.h>
#include <stdio.h>
#include "http_server_auth.h"
#include "http_req.h"

static bool
http_server_auth_ruuvi_get_cookie(
    const char *const p_cookies,
    const uint32_t    len_cookies,
    const char *const p_cookie_name,
    char *const       p_buf,
    const size_t      buf_size)
{
    p_buf[0]                         = '\0';
    const char *const p_cookie_begin = http_server_strnstr(p_cookies, p_cookie_name, len_cookies);
    if (NULL == p_cookie_begin)
    {
        return false;
    }
    const size_t len_till_eol    = len_cookies - (ptrdiff_t)(p_cookie_begin - p_cookies);
    const char * p_end_of_cookie = http_server_strnstr(p_cookie_begin, ";", len_till_eol);
    if (NULL == p_end_of_cookie)
    {
        p_end_of_cookie = p_cookie_begin + len_till_eol;
    }
    const size_t      len_of_cookie_pair = (size_t)(ptrdiff_t)(p_end_of_cookie - p_cookie_begin);
    const char *const p_delimiter        = http_server_strnstr(p_cookie_begin, "=", len_of_cookie_pair);
    if ((NULL == p_delimiter) || (p_delimiter > p_end_of_cookie))
    {
        return false;
    }
    const char *const p_cookie_value = p_delimiter + 1;
    const size_t      cookie_len     = (size_t)(ptrdiff_t)(p_end_of_cookie - p_cookie_value);
    if (0 == cookie_len)
    {
        return false;
    }
    if (cookie_len >= buf_size)
    {
        return false;
    }
    snprintf(p_buf, buf_size, "%.*s", cookie_len, p_cookie_value);
    return true;
}

static bool
http_server_auth_ruuvi_get_ruuvi_session_from_cookies(
    const char *const                          p_cookies,
    const uint32_t                             len_cookies,
    http_server_auth_ruuvi_session_id_t *const p_session_id)
{
    return http_server_auth_ruuvi_get_cookie(
        p_cookies,
        len_cookies,
        HTTP_SERVER_AUTH_RUUVI_COOKIE_SESSION,
        p_session_id->buf,
        sizeof(p_session_id->buf));
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

static bool
http_server_auth_ruuvi_get_ruuvi_prev_url_from_cookies(
    const char *const                        p_cookies,
    const uint32_t                           len_cookies,
    http_server_auth_ruuvi_prev_url_t *const p_prev_url)
{
    return http_server_auth_ruuvi_get_cookie(
        p_cookies,
        len_cookies,
        HTTP_SERVER_AUTH_RUUVI_COOKIE_PREV_URL,
        p_prev_url->buf,
        sizeof(p_prev_url->buf));
}

http_server_auth_ruuvi_prev_url_t
http_server_auth_ruuvi_get_prev_url_from_cookies(const http_req_header_t http_header)
{
    http_server_auth_ruuvi_prev_url_t prev_url = { 0 };

    uint32_t          len_cookie = 0;
    const char *const p_cookies  = http_req_header_get_field(http_header, "Cookie:", &len_cookie);
    if (NULL == p_cookies)
    {
        return prev_url;
    }
    if (!http_server_auth_ruuvi_get_ruuvi_prev_url_from_cookies(p_cookies, len_cookie, &prev_url))
    {
        return prev_url;
    }
    return prev_url;
}
