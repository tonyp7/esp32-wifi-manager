/**
 * @file http_server_handle_req_delete_auth.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_DELETE_AUTH_H
#define ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_DELETE_AUTH_H

#include "http_req.h"
#include "http_server_resp.h"
#include "http_server_auth.h"

#ifdef __cplusplus
extern "C" {
#endif

http_server_resp_t
http_server_handle_req_delete_auth(
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_DELETE_AUTH_H
