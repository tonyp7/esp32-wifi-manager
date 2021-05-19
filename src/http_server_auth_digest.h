/**
 * @file http_server_auth_digest.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_DIGEST_H
#define ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_DIGEST_H

#include "http_server_auth_common.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_AUTH_DIGEST_RANDOM_SIZE (32U)

#define HTTP_SERVER_AUTH_DIGEST_REALM_SIZE    (64U + 1U)
#define HTTP_SERVER_AUTH_DIGEST_NONCE_SIZE    (80U + 1U)
#define HTTP_SERVER_AUTH_DIGEST_URI_SIZE      (64U + 1U)
#define HTTP_SERVER_AUTH_DIGEST_RESPONSE_SIZE (64U + 1U)
#define HTTP_SERVER_AUTH_DIGEST_OPAQUE_SIZE   (80U + 1U)
#define HTTP_SERVER_AUTH_DIGEST_QOP_SIZE      (32U + 1U)
#define HTTP_SERVER_AUTH_DIGEST_NC_SIZE       (16U + 1U)
#define HTTP_SERVER_AUTH_DIGEST_CNONCE_SIZE   (32U + 1U)

typedef struct http_server_auth_digest_req_t
{
    /* Example of Authorization header:
     * Digest username="user1", realm="RuuviGatewayEEFF",
     * nonce="9689933745abb987e2cfae61d46f50c9efe2fbe9cfa6ad9c3ceb3c54fa2a2833",
     * uri="/auth",
     * response="32a8cf9eae6af8a897ed57a2c51f055d",
     * opaque="d3f1a85625217a33bdda63c646418c2be492100d9d1dec34d6e738c3a1766bc4",
     * qop=auth,
     * nc=00000001,
     * cnonce="3e48baed2616a1e9" */
    char username[HTTP_SERVER_MAX_AUTH_USER_LEN];
    char realm[HTTP_SERVER_AUTH_DIGEST_REALM_SIZE];
    char nonce[HTTP_SERVER_AUTH_DIGEST_NONCE_SIZE];
    char uri[HTTP_SERVER_AUTH_DIGEST_URI_SIZE];
    char response[HTTP_SERVER_AUTH_DIGEST_RESPONSE_SIZE];
    char opaque[HTTP_SERVER_AUTH_DIGEST_OPAQUE_SIZE];
    char qop[HTTP_SERVER_AUTH_DIGEST_QOP_SIZE];
    char nc[HTTP_SERVER_AUTH_DIGEST_NC_SIZE];
    char cnonce[HTTP_SERVER_AUTH_DIGEST_CNONCE_SIZE];
} http_server_auth_digest_req_t;

bool
http_server_parse_digest_authorization_str(
    const char *const                    p_authorization,
    const uint32_t                       len_authorization,
    http_server_auth_digest_req_t *const p_req);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_DIGEST_H
