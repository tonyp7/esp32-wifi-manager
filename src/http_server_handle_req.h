/**
 * @file http_server_handle_req.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_H
#define ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_H

#include "http_server_resp.h"
#include "http_req.h"
#include "http_server_auth.h"

#ifdef __cplusplus
extern "C" {
#endif

http_server_resp_t
http_server_handle_req(
    const http_req_info_t *const         p_req_info,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    http_header_extra_fields_t *const    p_extra_header_fields,
    const bool                           flag_access_from_lan);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_HTTP_SERVER_HANDLE_REQ_H
