/**
 * @file http_server_auth_common.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_COMMON_H
#define ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_MAX_AUTH_USER_LEN (64U + 1U)
#define HTTP_SERVER_MAX_AUTH_PASS_LEN (64U + 1U)

#define HTTP_SERVER_MAX_AUTH_API_KEY_LEN (64U + 1U)

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_HTTP_SERVER_AUTH_COMMON_H
