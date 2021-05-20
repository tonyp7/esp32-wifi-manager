/**
 * @file http_server_auth.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_auth.h"
#include "http_server_auth_type.h"
#include <string.h>
#include <stdio.h>

static http_server_auth_t      g_auth;
static http_server_auth_info_t g_auth_info;

void
http_server_auth_clear_info(void)
{
    memset(&g_auth, 0, sizeof(g_auth));
}

http_server_auth_digest_req_t *
http_server_auth_digest_get_info(void)
{
    return &g_auth.digest;
}

http_server_auth_ruuvi_t *
http_server_auth_ruuvi_get_info(void)
{
    return &g_auth.ruuvi;
}

const char *
http_server_strnstr(const char *const p_haystack, const char *const p_needle, const size_t len)
{
    const size_t needle_len = strnlen(p_needle, len);

    if (0 == needle_len)
    {
        return p_haystack;
    }

    const size_t max_offset = len - needle_len;

    for (size_t i = 0; i <= max_offset; ++i)
    {
        if (0 == strncmp(&p_haystack[i], p_needle, needle_len))
        {
            return &p_haystack[i];
        }
    }
    return NULL;
}

bool
http_server_set_auth(const char *const p_auth_type, const char *const p_auth_user, const char *const p_auth_pass)
{
    http_server_auth_type_e auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_DENY, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_DENY;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_ALLOW, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_ALLOW;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_BASIC, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_BASIC;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_DIGEST, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_DIGEST;
    }
    else if (0 == strcmp(HTTP_SERVER_AUTH_TYPE_STR_RUUVI, p_auth_type))
    {
        auth_type = HTTP_SERVER_AUTH_TYPE_RUUVI;
    }

    if ((NULL != p_auth_user) && (strlen(p_auth_user) >= sizeof(g_auth_info.auth_user)))
    {
        return false;
    }
    if ((NULL != p_auth_pass) && (strlen(p_auth_pass) >= sizeof(g_auth_info.auth_pass)))
    {
        return false;
    }
    g_auth_info.auth_type = auth_type;
    snprintf(g_auth_info.auth_user, sizeof(g_auth_info.auth_user), "%s", (NULL != p_auth_user) ? p_auth_user : "");
    snprintf(g_auth_info.auth_pass, sizeof(g_auth_info.auth_pass), "%s", (NULL != p_auth_pass) ? p_auth_pass : "");
    return true;
}

http_server_auth_info_t *
http_server_get_auth(void)
{
    return &g_auth_info;
}
