/**
 * @file http_server_auth_digest.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_auth_digest.h"
#include <stddef.h>
#include <string.h>
#include "http_server_auth.h"

static bool
http_server_parse_token(
    const char *const p_str,
    const uint32_t    len,
    const char *const p_prefix,
    const char *const p_suffix,
    char *const       p_buf,
    const size_t      buf_size)
{
    const char *p_idx1 = http_server_strnstr(p_str, p_prefix, len);
    if (NULL == p_idx1)
    {
        return false;
    }
    p_idx1 += strlen(p_prefix);
    const char *const p_idx2 = http_server_strnstr(
        p_idx1,
        p_suffix,
        (size_t)(len - (uint32_t)(ptrdiff_t)(p_idx1 - p_str)));
    if (NULL == p_idx2)
    {
        return false;
    }
    const size_t token_len = (size_t)(ptrdiff_t)(p_idx2 - p_idx1);
    if (token_len >= buf_size)
    {
        return false;
    }
    memcpy(p_buf, p_idx1, token_len);
    p_buf[token_len] = '\0';

    return true;
}

bool
http_server_parse_digest_authorization_str(
    const char *const                    p_authorization,
    const uint32_t                       len_authorization,
    http_server_auth_digest_req_t *const p_req)
{
    const char *const p_auth_prefix   = "Digest ";
    const size_t      auth_prefix_len = strlen(p_auth_prefix);
    if (0 != strncmp(p_authorization, p_auth_prefix, auth_prefix_len))
    {
        return false;
    }
    if (!http_server_parse_token(
            p_authorization,
            len_authorization,
            "username=\"",
            "\"",
            &p_req->username[0],
            sizeof(p_req->username)))
    {
        return false;
    }
    if (!http_server_parse_token(
            p_authorization,
            len_authorization,
            "realm=\"",
            "\"",
            &p_req->realm[0],
            sizeof(p_req->realm)))
    {
        return false;
    }
    if (!http_server_parse_token(
            p_authorization,
            len_authorization,
            "nonce=\"",
            "\"",
            &p_req->nonce[0],
            sizeof(p_req->nonce)))
    {
        return false;
    }
    if (!http_server_parse_token(
            p_authorization,
            len_authorization,
            "uri=\"",
            "\"",
            &p_req->uri[0],
            sizeof(p_req->uri)))
    {
        return false;
    }
    if (!http_server_parse_token(p_authorization, len_authorization, "qop=", ",", &p_req->qop[0], sizeof(p_req->qop)))
    {
        return false;
    }
    if (!http_server_parse_token(p_authorization, len_authorization, "nc=", ",", &p_req->nc[0], sizeof(p_req->nc)))
    {
        return false;
    }
    if (!http_server_parse_token(
            p_authorization,
            len_authorization,
            "cnonce=\"",
            "\"",
            &p_req->cnonce[0],
            sizeof(p_req->cnonce)))
    {
        return false;
    }
    if (!http_server_parse_token(
            p_authorization,
            len_authorization,
            "response=\"",
            "\"",
            &p_req->response[0],
            sizeof(p_req->response)))
    {
        return false;
    }
    if (!http_server_parse_token(
            p_authorization,
            len_authorization,
            "opaque=\"",
            "\"",
            &p_req->opaque[0],
            sizeof(p_req->opaque)))
    {
        return false;
    }
    return true;
}
