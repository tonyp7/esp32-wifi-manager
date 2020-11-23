#include <limits.h>
/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file http_server.c
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "http_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "lwip/netbuf.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/opt.h"
#include "lwip/memp.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/priv/api_msg.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/priv/tcpip_priv.h"

#include "wifi_manager_internal.h"
#include "wifi_manager.h"
#include "json_access_points.h"
#include "json_network_info.h"

#include "cJSON.h"
#include "sta_ip_safe.h"
#include "os_task.h"
#include "app_malloc.h"
#include "str_buf.h"
#include "wifi_sta_config.h"
#include "http_req.h"
#include "esp_type_wrapper.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "log.h"

#define FULLBUF_SIZE (4U * 1024U)

/**
 * @brief RTOS task for the HTTP server. Do not start manually.
 * @see void http_server_start()
 */
static _Noreturn void
http_server_task(void *p_param);

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "http_server";

/* @brief task handle for the http server */
static os_task_handle_t gh_http_task;

ATTR_PRINTF(3, 4)
static void
http_server_netconn_printf(struct netconn *p_conn, const bool flag_more, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    str_buf_t str_buf = str_buf_vprintf_with_alloc(fmt, args);
    va_end(args);
    if (NULL == str_buf.buf)
    {
        return;
    }
    ESP_LOGD(TAG, "Respond: %s", str_buf.buf);
    uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
    if (flag_more)
    {
        netconn_flags |= (uint8_t)NETCONN_MORE;
    }
    netconn_write(p_conn, str_buf.buf, str_buf_get_len(&str_buf), netconn_flags);
    str_buf_free_buf(&str_buf);
}

static const char *
http_get_content_type_str(const http_content_type_e content_type)
{
    const char *content_type_str = "application/octet-stream";
    switch (content_type)
    {
        case HTTP_CONENT_TYPE_TEXT_HTML:
            content_type_str = "text/html";
            break;
        case HTTP_CONENT_TYPE_TEXT_PLAIN:
            content_type_str = "text/plain";
            break;
        case HTTP_CONENT_TYPE_TEXT_CSS:
            content_type_str = "text/css";
            break;
        case HTTP_CONENT_TYPE_TEXT_JAVASCRIPT:
            content_type_str = "text/javascript";
            break;
        case HTTP_CONENT_TYPE_IMAGE_PNG:
            content_type_str = "image/png";
            break;
        case HTTP_CONENT_TYPE_IMAGE_SVG_XML:
            content_type_str = "image/svg+xml";
            break;
        case HTTP_CONENT_TYPE_APPLICATION_JSON:
            content_type_str = "application/json";
            break;
        case HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM:
            content_type_str = "application/octet-stream";
            break;
    }
    return content_type_str;
}

const char *
http_get_content_encoding_str(const http_server_resp_t *p_resp)
{
    const char *content_encoding_str = "";
    switch (p_resp->content_encoding)
    {
        case HTTP_CONENT_ENCODING_NONE:
            content_encoding_str = "";
            break;
        case HTTP_CONENT_ENCODING_GZIP:
            content_encoding_str = "Content-Encoding: gzip\n";
            break;
    }
    return content_encoding_str;
}

const char *
http_get_cache_control_str(const http_server_resp_t *p_resp)
{
    const char *cache_control_str = "";
    if (p_resp->flag_no_cache)
    {
        cache_control_str
            = "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
              "Pragma: no-cache\r\n";
    }
    return cache_control_str;
}

static void
write_content_from_flash(struct netconn *p_conn, const http_server_resp_t *p_resp)
{
    const err_t err = netconn_write(p_conn, p_resp->select_location.memory.p_buf, p_resp->content_len, NETCONN_NOCOPY);
    if (ERR_OK != err)
    {
        ESP_LOGE(TAG, "netconn_write failed, err=%d", err);
    }
}

static void
write_content_from_static_mem(struct netconn *p_conn, const http_server_resp_t *p_resp)
{
    const err_t err = netconn_write(p_conn, p_resp->select_location.memory.p_buf, p_resp->content_len, NETCONN_COPY);
    if (ERR_OK != err)
    {
        ESP_LOGE(TAG, "netconn_write failed, err=%d", err);
    }
}

static void
write_content_from_heap(struct netconn *p_conn, http_server_resp_t *p_resp)
{
    const err_t err = netconn_write(p_conn, p_resp->select_location.memory.p_buf, p_resp->content_len, NETCONN_COPY);
    if (ERR_OK != err)
    {
        ESP_LOGE(TAG, "netconn_write failed, err=%d", err);
    }
    app_free_const_pptr((const void **)&p_resp->select_location.memory.p_buf);
}

static void
write_content_from_fatfs(struct netconn *p_conn, const http_server_resp_t *p_resp)
{
    static char tmp_buf[512U];
    uint32_t    rem_len = p_resp->content_len;
    while (rem_len > 0)
    {
        const uint32_t num_bytes       = (rem_len <= sizeof(tmp_buf)) ? rem_len : sizeof(tmp_buf);
        const bool     flag_last_block = (num_bytes == rem_len) ? true : false;

        const file_read_result_t read_result = read(p_resp->select_location.fatfs.fd, tmp_buf, num_bytes);
        if (read_result < 0)
        {
            ESP_LOGE(TAG, "Failed to read %u bytes", num_bytes);
            break;
        }
        if (read_result != num_bytes)
        {
            ESP_LOGE(TAG, "Read %u bytes, while requested %u bytes", read_result, num_bytes);
            break;
        }
        rem_len -= read_result;
        uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
        if (!flag_last_block)
        {
            netconn_flags |= (uint8_t)NETCONN_MORE;
        }
        ESP_LOGD(TAG, "Send %u bytes", num_bytes);
        const err_t err = netconn_write(p_conn, tmp_buf, num_bytes, netconn_flags);
        if (ERR_OK != err)
        {
            ESP_LOGE(TAG, "netconn_write failed, err=%d", err);
            break;
        }
    }
    ESP_LOGD(TAG, "Close file fd=%d", p_resp->select_location.fatfs.fd);
    close(p_resp->select_location.fatfs.fd);
}

static void
http_server_netconn_resp_200(struct netconn *p_conn, http_server_resp_t *p_resp)
{
    const bool use_extra_content_type_param = (NULL != p_resp->p_content_type_param)
                                              && ('\0' != p_resp->p_content_type_param[0]);

    http_server_netconn_printf(
        p_conn,
        true,
        "HTTP/1.1 200 OK\n"
        "Content-type: %s; charset=utf-8%s%s\n"
        "Content-Length: %lu\n"
        "%s"
        "%s"
        "\n",
        http_get_content_type_str(p_resp->content_type),
        use_extra_content_type_param ? "; " : "",
        use_extra_content_type_param ? p_resp->p_content_type_param : "",
        (printf_ulong_t)p_resp->content_len,
        http_get_content_encoding_str(p_resp),
        http_get_cache_control_str(p_resp));

    switch (p_resp->content_location)
    {
        case HTTP_CONTENT_LOCATION_NO_CONTENT:
            break;
        case HTTP_CONTENT_LOCATION_FLASH_MEM:
            write_content_from_flash(p_conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_STATIC_MEM:
            write_content_from_static_mem(p_conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_HEAP:
            write_content_from_heap(p_conn, p_resp);
            break;
        case HTTP_CONTENT_LOCATION_FATFS:
            write_content_from_fatfs(p_conn, p_resp);
            break;
    }
}

static http_server_resp_t
http_server_resp_200_json(const char *p_json_content)
{
    const bool flag_no_cache = true;
    return http_server_resp_data_in_static_mem(
        HTTP_CONENT_TYPE_APPLICATION_JSON,
        NULL,
        strlen(p_json_content),
        HTTP_CONENT_ENCODING_NONE,
        (const uint8_t *)p_json_content,
        flag_no_cache);
}

static void
http_server_netconn_resp_302(struct netconn *p_conn)
{
    ESP_LOGI(TAG, "Respond: 302 Found");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 302 Found\n"
        "Location: http://%s/\n"
        "\n",
        DEFAULT_AP_IP);
}

static void
http_server_netconn_resp_400(struct netconn *p_conn)
{
    ESP_LOGW(TAG, "Respond: 400 Bad Request");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 400 Bad Request\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

static void
http_server_netconn_resp_404(struct netconn *p_conn)
{
    ESP_LOGW(TAG, "Respond: 404 Not Found");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

static void
http_server_netconn_resp_503(struct netconn *p_conn)
{
    ESP_LOGW(TAG, "Respond: 503 Service Unavailable");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

void
http_server_start(void)
{
    if (NULL == gh_http_task)
    {
        ESP_LOGI(TAG, "Run http_server");
        const uint32_t stack_depth = 20U * 1024U;
        if (!os_task_create(
                &http_server_task,
                "http_server",
                stack_depth,
                NULL,
                WIFI_MANAGER_TASK_PRIORITY - 1,
                &gh_http_task))
        {
            ESP_LOGE(TAG, "xTaskCreate failed: http_server");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Run http_server - the server is already running");
    }
}

void
http_server_stop(void)
{
    if (NULL != gh_http_task)
    {
        vTaskDelete(gh_http_task);
        gh_http_task = NULL;
    }
}

static _Noreturn void
http_server_task(ATTR_UNUSED void *p_param)
{
    struct netconn *p_conn    = netconn_new(NETCONN_TCP);
    const u16_t     http_port = 80U;
    netconn_bind(p_conn, IP_ADDR_ANY, http_port);
    netconn_listen(p_conn);
    ESP_LOGI(TAG, "HTTP Server listening on 80/tcp");
    for (;;)
    {
        struct netconn *p_new_conn = NULL;
        const err_t     err        = netconn_accept(p_conn, &p_new_conn);
        if (ERR_OK == err)
        {
            http_server_netconn_serve(p_new_conn);
            netconn_close(p_new_conn);
            netconn_delete(p_new_conn);
        }
        else if (ERR_TIMEOUT == err)
        {
            ESP_LOGE(TAG, "http_server: netconn_accept ERR_TIMEOUT");
        }
        else if (ERR_ABRT == err)
        {
            ESP_LOGE(TAG, "http_server: netconn_accept ERR_ABRT");
        }
        else
        {
            ESP_LOGE(TAG, "http_server: netconn_accept: %d", err);
        }
        taskYIELD(); /* allows the freeRTOS scheduler to take over if needed. */
    }
    netconn_close(p_conn);
    netconn_delete(p_conn);

    vTaskDelete(NULL);
}

static const char *
get_http_body(const char *msg, uint32_t len, uint32_t *p_body_len)
{
    static const char g_newlines[] = "\r\n\r\n";

    const char *p_body = strstr(msg, g_newlines);
    if (NULL == p_body)
    {
        ESP_LOGD(TAG, "http body not found: %s", msg);
        return 0;
    }
    p_body += strlen(g_newlines);
    if (NULL != p_body_len)
    {
        *p_body_len = len - (uint32_t)(ptrdiff_t)(p_body - msg);
    }
    return p_body;
}

static http_server_resp_t
http_server_handle_req_get(const char *p_file_name)
{
    ESP_LOGI(TAG, "GET /%s", p_file_name);

    const char *file_ext = strrchr(p_file_name, '.');
    if ((NULL != file_ext) && (0 == strcmp(file_ext, ".json")))
    {
        if (0 == strcmp(p_file_name, "ap.json"))
        {
            /* if we can get the mutex, write the last version of the AP list */
            const TickType_t ticks_to_wait = 10U;
            if (!wifi_manager_lock_json_buffer(ticks_to_wait))
            {
                ESP_LOGD(TAG, "http_server_netconn_serve: GET /ap.json failed to obtain mutex");
                ESP_LOGI(TAG, "ap.json: 503");
                return http_server_resp_503();
            }
            const char *buff = json_access_points_get();
            if (NULL == buff)
            {
                ESP_LOGI(TAG, "status.json: 503");
                return http_server_resp_503();
            }
            ESP_LOGI(TAG, "ap.json: %s", buff);
            const http_server_resp_t resp = http_server_resp_200_json(buff);
            wifi_manager_unlock_json_buffer();
            return resp;
        }
        else if (0 == strcmp(p_file_name, "status.json"))
        {
            const TickType_t ticks_to_wait = 10U;
            if (!wifi_manager_lock_json_buffer(ticks_to_wait))
            {
                ESP_LOGD(TAG, "http_server_netconn_serve: GET /status failed to obtain mutex");
                ESP_LOGI(TAG, "status.json: 503");
                return http_server_resp_503();
            }
            const char *buff = json_network_info_get();
            if (NULL == buff)
            {
                ESP_LOGI(TAG, "status.json: 503");
                return http_server_resp_503();
            }
            ESP_LOGI(TAG, "status.json: %s", buff);
            const http_server_resp_t resp = http_server_resp_200_json(buff);
            wifi_manager_unlock_json_buffer();
            return resp;
        }
    }
    return wifi_manager_cb_on_http_get(p_file_name);
}

static http_server_resp_t
http_server_handle_req_delete(const char *p_file_name)
{
    ESP_LOGI(TAG, "DELETE /%s", p_file_name);
    if (0 == strcmp(p_file_name, "connect.json"))
    {
        ESP_LOGD(TAG, "http_server_netconn_serve: DELETE /connect.json");
        /* request a disconnection from wifi and forget about it */
        wifi_manager_disconnect_async();
        return http_server_resp_200_json("{}");
    }
    return wifi_manager_cb_on_http_delete(p_file_name);
}

static http_server_resp_t
http_server_handle_req_post_connect_json(const http_req_header_t http_header)
{
    ESP_LOGD(TAG, "http_server_netconn_serve: POST /connect.json");
    uint32_t    len_ssid     = 0;
    uint32_t    len_password = 0;
    const char *p_ssid       = http_req_header_get_field(http_header, "X-Custom-ssid:", &len_ssid);
    const char *p_password   = http_req_header_get_field(http_header, "X-Custom-pwd:", &len_password);
    if ((NULL != p_ssid) && (len_ssid <= MAX_SSID_SIZE) && (NULL != p_password) && (len_password <= MAX_PASSWORD_SIZE))
    {
        wifi_sta_config_set_ssid_and_password(p_ssid, len_ssid, p_password, len_password);

        ESP_LOGD(TAG, "http_server_netconn_serve: wifi_manager_connect_async() call");
        wifi_manager_connect_async();
        return http_server_resp_200_json("{}");
    }
    else
    {
        /* bad request the authentification header is not complete/not the correct format */
        return http_server_resp_400();
    }
}

static http_server_resp_t
http_server_handle_req_post(
    const char *            p_file_name,
    const http_req_header_t http_header,
    const http_req_body_t   http_body)
{
    ESP_LOGI(TAG, "POST /%s", p_file_name);
    if (0 == strcmp(p_file_name, "connect.json"))
    {
        return http_server_handle_req_post_connect_json(http_header);
    }
    return wifi_manager_cb_on_http_post(p_file_name, http_body);
}

static http_server_resp_t
http_server_handle_req(const http_req_info_t *p_req_info)
{
    const char *path = p_req_info->http_uri.ptr;
    if ('/' == path[0])
    {
        path += 1;
    }

    if (0 == strcmp("GET", p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_get(path);
    }
    else if (0 == strcmp("DELETE", p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_delete(path);
    }
    else if (0 == strcmp("POST", p_req_info->http_cmd.ptr))
    {
        return http_server_handle_req_post(path, p_req_info->http_header, p_req_info->http_body);
    }
    else
    {
        return http_server_resp_400();
    }
}

static bool
http_server_recv_and_handle(struct netconn *p_conn, char *p_req_buf, uint32_t *p_req_size, bool *p_req_ready)
{
    struct netbuf *p_netbuf_in = NULL;

    const err_t err = netconn_recv(p_conn, &p_netbuf_in);
    if (ERR_OK != err)
    {
        LOG_ERR("netconn recv: %d", err);
        return false;
    }

    char *p_buf  = NULL;
    u16_t buflen = 0;
    netbuf_data(p_netbuf_in, (void **)&p_buf, &buflen);

    if ((*p_req_size + buflen) > FULLBUF_SIZE)
    {
        ESP_LOGW(TAG, "fullbuf full, fullsize: %u, buflen: %d", *p_req_size, buflen);
        netbuf_delete(p_netbuf_in);
        return false;
    }
    memcpy(&p_req_buf[*p_req_size], p_buf, buflen);
    *p_req_size += buflen;
    p_req_buf[*p_req_size] = 0; // zero terminated string

    netbuf_delete(p_netbuf_in);

    // check if there should be more data coming from conn
    uint32_t                field_len  = 0;
    const http_req_header_t req_header = {
        .ptr = p_req_buf,
    };
    const char *p_content_len_str = http_req_header_get_field(req_header, "Content-Length:", &field_len);
    if (NULL != p_content_len_str)
    {
        const uint32_t content_len = (uint32_t)strtoul(p_content_len_str, NULL, 10);
        uint32_t       body_len    = 0;
        const char *   p_body      = get_http_body(p_req_buf, *p_req_size, &body_len);
        if (NULL != p_body)
        {
            ESP_LOGD(TAG, "Header Content-Length: %d, HTTP body length: %d", content_len, body_len);
            if (content_len == body_len)
            {
                // HTTP request is full
                *p_req_ready = true;
            }
            else
            {
                ESP_LOGD(TAG, "request not full yet");
            }
        }
        else
        {
            ESP_LOGD(TAG, "Header Content-Length: %d, body not found", content_len);
            // read more data
        }
    }
    else
    {
        *p_req_ready = true;
    }
    return true;
}

void
http_server_netconn_serve(struct netconn *p_conn)
{
    char     req_buf[FULLBUF_SIZE + 1];
    uint32_t req_size  = 0;
    bool     req_ready = false;

    while (!req_ready)
    {
        if (!http_server_recv_and_handle(p_conn, &req_buf[0], &req_size, &req_ready))
        {
            break;
        }
    }

    if (!req_ready)
    {
        ESP_LOGW(TAG, "the connection was closed by the client side");
        return;
    }
    ESP_LOGD(TAG, "req: %s", req_buf);

    const http_req_info_t req_info = http_req_parse(req_buf);

    ESP_LOGD(TAG, "p_http_cmd: %s", req_info.http_cmd.ptr);
    ESP_LOGD(TAG, "p_http_uri: %s", req_info.http_uri.ptr);
    ESP_LOGD(TAG, "p_http_ver: %s", req_info.http_ver.ptr);
    ESP_LOGD(TAG, "p_http_header: %s", req_info.http_header.ptr);
    ESP_LOGD(TAG, "p_http_body: %s", req_info.http_body.ptr);

    if (!req_info.is_success)
    {
        http_server_netconn_resp_400(p_conn);
        return;
    }
    /* captive portal functionality: redirect to access point IP for HOST that are not the access point IP OR the
     * STA IP */
    uint32_t    host_len = 0;
    const char *p_host   = http_req_header_get_field(req_info.http_header, "Host:", &host_len);
    /* determine if Host is from the STA IP address */

    const sta_ip_string_t ip_str  = sta_ip_safe_get(portMAX_DELAY);
    const bool access_from_sta_ip = ('\0' == ip_str.buf[0]) || ((host_len > 0) && (NULL != strstr(p_host, ip_str.buf)));

    ESP_LOGD(TAG, "Host: %.*s", host_len, p_host);
    ESP_LOGD(TAG, "StaticIP: %s", ip_str.buf);
    if ((host_len > 0) && (NULL == strstr(p_host, DEFAULT_AP_IP)) && (!access_from_sta_ip))
    {
        http_server_netconn_resp_302(p_conn);
        return;
    }
    http_server_resp_t resp = http_server_handle_req(&req_info);
    switch (resp.http_resp_code)
    {
        case HTTP_RESP_CODE_200:
            http_server_netconn_resp_200(p_conn, &resp);
            return;
        case HTTP_RESP_CODE_400:
            http_server_netconn_resp_400(p_conn);
            return;
        case HTTP_RESP_CODE_404:
            http_server_netconn_resp_404(p_conn);
            return;
        case HTTP_RESP_CODE_503:
            http_server_netconn_resp_503(p_conn);
            return;
    }
    assert(0);
    http_server_netconn_resp_503(p_conn);
}
