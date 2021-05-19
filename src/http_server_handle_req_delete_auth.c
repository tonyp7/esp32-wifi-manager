/**
 * @file http_server_handle_req_delete_auth.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_handle_req_delete_auth.h"
#include "http_server_auth.h"

http_server_resp_t
http_server_handle_req_delete_auth(
    const http_req_header_t              http_header,
    const sta_ip_string_t *const         p_remote_ip,
    const http_server_auth_info_t *const p_auth_info,
    const wifi_ssid_t *const             p_ap_ssid,
    http_header_extra_fields_t *const    p_extra_header_fields)
{
    if (HTTP_SERVER_AUTH_TYPE_RUUVI != p_auth_info->auth_type)
    {
        return http_server_resp_503();
    }
    http_server_auth_ruuvi_session_id_t session_id = { 0 };
    if (!http_server_auth_ruuvi_get_session_id_from_cookies(http_header, &session_id))
    {
        return http_server_resp_401_auth_ruuvi(p_remote_ip, p_ap_ssid, p_extra_header_fields);
    }
    http_server_auth_ruuvi_authorized_session_t *const p_authorized_session
        = http_server_auth_ruuvi_find_authorized_session(&session_id, p_remote_ip);

    if (NULL == p_authorized_session)
    {
        return http_server_resp_401_auth_ruuvi(p_remote_ip, p_ap_ssid, p_extra_header_fields);
    }

    p_authorized_session->session_id.buf[0] = '\0';

    return http_server_resp_200_json("{}");
}
