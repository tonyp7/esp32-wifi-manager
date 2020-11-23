/**
 * @file http_server_resp.h
 * @author TheSomeMan
 * @date 2020-10-31
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_resp.h"

static http_server_resp_t
http_server_resp_err(const http_resp_code_e http_resp_code)
{
    const http_server_resp_t resp = {
        .http_resp_code       = http_resp_code,
        .content_location     = HTTP_CONTENT_LOCATION_NO_CONTENT,
        .flag_no_cache        = false,
        .content_type         = HTTP_CONENT_TYPE_TEXT_HTML,
        .p_content_type_param = NULL,
        .content_len          = 0,
        .content_encoding     = HTTP_CONENT_ENCODING_NONE,
        .select_location = {
            .memory = {
                .p_buf = NULL,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_server_resp_400(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_400);
}

http_server_resp_t
http_server_resp_404(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_404);
}

http_server_resp_t
http_server_resp_503(void)
{
    return http_server_resp_err(HTTP_RESP_CODE_503);
}

http_server_resp_t
http_server_resp_data_in_flash(
    const http_content_type_e     content_type,
    const char *                  p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t *               p_buf)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_200,
        .content_location     = HTTP_CONTENT_LOCATION_FLASH_MEM,
        .flag_no_cache        = false,
        .content_type         = content_type,
        .p_content_type_param = p_content_type_param,
        .content_len          = content_len,
        .content_encoding     = content_encoding,
        .select_location      = {
            .memory = {
                .p_buf = p_buf,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_server_resp_data_in_static_mem(
    const http_content_type_e     content_type,
    const char *                  p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t *               p_buf,
    const bool                    flag_no_cache)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_200,
        .content_location     = HTTP_CONTENT_LOCATION_STATIC_MEM,
        .flag_no_cache        = flag_no_cache,
        .content_type         = content_type,
        .p_content_type_param = p_content_type_param,
        .content_len          = content_len,
        .content_encoding     = content_encoding,
        .select_location      = {
            .memory = {
                .p_buf = p_buf,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_server_resp_data_in_heap(
    const http_content_type_e     content_type,
    const char *                  p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const uint8_t *               p_buf,
    const bool                    flag_no_cache)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_200,
        .content_location     = HTTP_CONTENT_LOCATION_HEAP,
        .flag_no_cache        = flag_no_cache,
        .content_type         = content_type,
        .p_content_type_param = p_content_type_param,
        .content_len          = content_len,
        .content_encoding     = content_encoding,
        .select_location      = {
            .memory = {
                .p_buf = p_buf,
            },
        },
    };
    return resp;
}

http_server_resp_t
http_server_resp_data_from_file(
    const http_content_type_e     content_type,
    const char *                  p_content_type_param,
    const size_t                  content_len,
    const http_content_encoding_e content_encoding,
    const socket_t                fd)
{
    const http_server_resp_t resp = {
        .http_resp_code       = HTTP_RESP_CODE_200,
        .content_location     = HTTP_CONTENT_LOCATION_FATFS,
        .flag_no_cache        = false,
        .content_type         = content_type,
        .p_content_type_param = p_content_type_param,
        .content_len          = content_len,
        .content_encoding     = content_encoding,
        .select_location      = {
            .fatfs = {
                .fd = fd,
            },
        },
    };
    return resp;
}
