/**
 * @file http_req.h
 * @author TheSomeMan
 * @date 2020-11-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFI_MANAGER_HTTP_REQ_H
#define WIFI_MANAGER_HTTP_REQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct http_req_cmd_t
{
    const char *ptr;
} http_req_cmd_t;

typedef struct http_req_uri_t
{
    const char *ptr;
} http_req_uri_t;

typedef struct http_req_uri_params_t
{
    const char *ptr;
} http_req_uri_params_t;

typedef struct http_req_ver_t
{
    const char *ptr;
} http_req_ver_t;

typedef struct http_req_header_t
{
    const char *ptr;
} http_req_header_t;

typedef struct http_req_body_t
{
    const char *ptr;
} http_req_body_t;

typedef struct http_req_info_t
{
    bool                  is_success;
    http_req_cmd_t        http_cmd;
    http_req_uri_t        http_uri;
    http_req_uri_params_t http_uri_params;
    http_req_ver_t        http_ver;
    http_req_header_t     http_header;
    http_req_body_t       http_body;
} http_req_info_t;

http_req_info_t
http_req_parse(char *p_req_buf);

const char *
http_req_header_get_field(const http_req_header_t req_header, const char *const p_field_name, uint32_t *const p_len);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_HTTP_REQ_H
