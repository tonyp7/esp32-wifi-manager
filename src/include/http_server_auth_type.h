/**
 * @file http_server_auth_type.h
 * @author TheSomeMan
 * @date 2021-05-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFI_MANAGER_HTTP_SERVER_AUTH_TYPE_H
#define WIFI_MANAGER_HTTP_SERVER_AUTH_TYPE_H

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_AUTH_TYPE_STR_ALLOW  "lan_auth_allow"
#define HTTP_SERVER_AUTH_TYPE_STR_BASIC  "lan_auth_basic"
#define HTTP_SERVER_AUTH_TYPE_STR_DIGEST "lan_auth_digest"
#define HTTP_SERVER_AUTH_TYPE_STR_RUUVI  "lan_auth_ruuvi"
#define HTTP_SERVER_AUTH_TYPE_STR_DENY   "lan_auth_deny"

typedef enum http_server_auth_type_e
{
    HTTP_SERVER_AUTH_TYPE_ALLOW  = 0,
    HTTP_SERVER_AUTH_TYPE_BASIC  = 1,
    HTTP_SERVER_AUTH_TYPE_DIGEST = 2,
    HTTP_SERVER_AUTH_TYPE_RUUVI  = 3,
    HTTP_SERVER_AUTH_TYPE_DENY   = 4,
} http_server_auth_type_e;

bool
http_server_set_auth(const char *const p_auth_type, const char *const p_auth_user, const char *const p_auth_pass);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_HTTP_SERVER_AUTH_TYPE_H
