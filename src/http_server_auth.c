/**
 * @file http_server_auth.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_auth.h"
#include <string.h>

static http_server_auth_t g_auth;

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
