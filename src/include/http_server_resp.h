/**
 * @file http_server_resp.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFI_MANAGER_HTTP_SERVER_RESP_H
#define WIFI_MANAGER_HTTP_SERVER_RESP_H

#include "wifi_manager_defs.h"
#include "esp_type_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

http_server_resp_t
http_server_resp_400(void);

http_server_resp_t
http_server_resp_404(void);

http_server_resp_t
http_server_resp_503(void);

http_server_resp_t
http_server_resp_data_in_flash(
    const http_content_type_e     content_type,
    const char *                  p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t *               p_buf);

http_server_resp_t
http_server_resp_data_in_static_mem(
    const http_content_type_e     content_type,
    const char *                  p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t *               p_buf,
    const bool                    flag_no_cache);

http_server_resp_t
http_server_resp_data_in_heap(
    const http_content_type_e     content_type,
    const char *                  p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t *               p_buf,
    const bool                    flag_no_cache);

http_server_resp_t
http_server_resp_data_from_file(
    const http_content_type_e     content_type,
    const char *                  p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const socket_t                fd);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_HTTP_SERVER_RESP_H
