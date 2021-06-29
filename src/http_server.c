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
#include "os_malloc.h"
#include "str_buf.h"
#include "http_req.h"
#include "esp_type_wrapper.h"
#include "os_signal.h"
#include "os_mutex.h"
#include "wifiman_msg.h"
#include "http_server_handle_req_get_auth.h"
#include "http_server_handle_req.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define HTTP_SERVER_SIG_STOP (OS_SIGNAL_NUM_0)

#define FULLBUF_SIZE (4U * 1024U)

#define HTTP_SERVER_ACCEPT_TIMEOUT_MS (1 * 1000)

#define HTTP_SERVER_STATUS_JSON_REQUEST_TIMEOUT_MS (20 * 1000)
#define HTTP_SERVER_STA_AP_TIMEOUT_MS              (60 * 1000)

#define HTTP_HEADER_DATE_EXAMPLE "Date: Thu, 01 Jan 2021 00:00:00 GMT\r\n"

typedef struct http_header_date_str_t
{
    char buf[sizeof(HTTP_HEADER_DATE_EXAMPLE)];
} http_header_date_str_t;

/**
 * @brief RTOS task for the HTTP server. Do not start manually.
 * @see void http_server_start()
 */
static void
http_server_task(void);

/**
 * @brief Helper function that processes one HTTP request at a time.
 * @param p_conn - ptr to a connection object
 */
static void
http_server_netconn_serve(struct netconn *p_conn);

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "http_server";

static os_mutex_static_t  g_http_server_mutex_mem;
static os_mutex_t         g_http_server_mutex;
static os_signal_static_t g_http_server_signal_mem;
static os_signal_t *      gp_http_server_sig;
static os_delta_ticks_t   g_timestamp_last_http_status_request;
static bool               g_is_ap_sta_ip_assigned;
static bool               g_http_server_disable_ap_stopping_by_timeout;

static http_header_extra_fields_t g_http_server_extra_header_fields;

void
http_server_init(void)
{
    assert(NULL == g_http_server_mutex);
    g_http_server_mutex = os_mutex_create_static(&g_http_server_mutex_mem);
    gp_http_server_sig  = os_signal_create_static(&g_http_server_signal_mem);
    os_signal_add(gp_http_server_sig, HTTP_SERVER_SIG_STOP);
}

void
http_server_disable_ap_stopping_by_timeout(void)
{
    g_http_server_disable_ap_stopping_by_timeout = true;
}

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
    LOG_DBG("Respond: %s", str_buf.buf);
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
            content_encoding_str = "Content-Encoding: gzip\r\n";
            break;
    }
    return content_encoding_str;
}

static const char *
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
        LOG_ERR("netconn_write failed, err=%d", err);
    }
}

static void
write_content_from_static_mem(struct netconn *p_conn, const http_server_resp_t *p_resp)
{
    const err_t err = netconn_write(p_conn, p_resp->select_location.memory.p_buf, p_resp->content_len, NETCONN_COPY);
    if (ERR_OK != err)
    {
        LOG_ERR("netconn_write failed, err=%d", err);
    }
}

static void
write_content_from_heap(struct netconn *p_conn, http_server_resp_t *p_resp)
{
    const err_t err = netconn_write(p_conn, p_resp->select_location.memory.p_buf, p_resp->content_len, NETCONN_COPY);
    if (ERR_OK != err)
    {
        LOG_ERR("netconn_write failed, err=%d", err);
    }
    os_free(p_resp->select_location.memory.p_buf);
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
            LOG_ERR("Failed to read %u bytes", num_bytes);
            break;
        }
        if (read_result != num_bytes)
        {
            LOG_ERR("Read %u bytes, while requested %u bytes", read_result, num_bytes);
            break;
        }
        rem_len -= read_result;
        uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
        if (!flag_last_block)
        {
            netconn_flags |= (uint8_t)NETCONN_MORE;
        }
        LOG_VERBOSE("Send %u bytes", num_bytes);
        const err_t err = netconn_write(p_conn, tmp_buf, num_bytes, netconn_flags);
        if (ERR_OK != err)
        {
            LOG_ERR("netconn_write failed, err=%d", err);
            break;
        }
    }
    LOG_DBG("Close file fd=%d", p_resp->select_location.fatfs.fd);
    close(p_resp->select_location.fatfs.fd);
}

static http_header_date_str_t
http_server_gen_header_date_str(const bool flag_gen_date)
{
    http_header_date_str_t date_str = { 0 };
    if (flag_gen_date)
    {
        const time_t cur_time = time(NULL);
        struct tm    tm_time  = { 0 };
        gmtime_r(&cur_time, &tm_time);
        strftime(date_str.buf, sizeof(date_str.buf), "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", &tm_time);
    }
    return date_str;
}

static void
http_server_netconn_resp_200(struct netconn *p_conn, http_server_resp_t *p_resp)
{
    const bool use_extra_content_type_param = (NULL != p_resp->p_content_type_param)
                                              && ('\0' != p_resp->p_content_type_param[0]);
    const http_header_date_str_t date_str = http_server_gen_header_date_str(p_resp->flag_add_header_date);

    http_server_netconn_printf(
        p_conn,
        true,
        "HTTP/1.1 200 OK\r\n"
        "Server: Ruuvi Gateway\r\n"
        "%s"
        "Content-type: %s; charset=utf-8%s%s\r\n"
        "Content-Length: %lu\r\n"
        "%s"
        "%s"
        "\r\n",
        date_str.buf,
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

static void
http_server_netconn_resp_302(struct netconn *p_conn)
{
    LOG_INFO("Respond: 302 Found");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 302 Found\r\n"
        "Server: Ruuvi Gateway\r\n"
        "Location: http://%s/\r\n"
        "\r\n",
        DEFAULT_AP_IP);
}

static void
http_server_netconn_resp_302_auth_html(struct netconn *p_conn, const sta_ip_string_t *const p_ip_str)
{
    LOG_INFO("Respond: 302 Found");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 302 Found\r\n"
        "Server: Ruuvi Gateway\r\n"
        "Location: http://%s/auth.html\r\n"
        "\r\n",
        p_ip_str->buf);
}

static void
http_server_netconn_resp_400(struct netconn *p_conn)
{
    LOG_WARN("Respond: 400 Bad Request");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 400 Bad Request\r\n"
        "Server: Ruuvi Gateway\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

static void
http_server_netconn_resp_401(
    struct netconn *                        p_conn,
    http_server_resp_t *                    p_resp,
    const http_header_extra_fields_t *const p_extra_header_fields)
{
    LOG_INFO("Respond: 401 Unauthorized");
    const bool use_extra_content_type_param = (NULL != p_resp->p_content_type_param)
                                              && ('\0' != p_resp->p_content_type_param[0]);
    const http_header_date_str_t date_str = http_server_gen_header_date_str(true);
    http_server_netconn_printf(
        p_conn,
        true,
        "HTTP/1.1 401 Unauthorized\r\n"
        "Server: Ruuvi Gateway\r\n"
        "%s"
        "Content-type: %s; charset=utf-8%s%s\r\n"
        "Content-Length: %lu\r\n"
        "%s"
        "%s"
        "%s"
        "\r\n",
        date_str.buf,
        http_get_content_type_str(p_resp->content_type),
        use_extra_content_type_param ? "; " : "",
        use_extra_content_type_param ? p_resp->p_content_type_param : "",
        (printf_ulong_t)p_resp->content_len,
        (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : "",
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

static void
http_server_netconn_resp_403(
    struct netconn *                        p_conn,
    http_server_resp_t *                    p_resp,
    const http_header_extra_fields_t *const p_extra_header_fields)
{
    LOG_INFO("Respond: 403 Forbidden");
    const bool use_extra_content_type_param = (NULL != p_resp->p_content_type_param)
                                              && ('\0' != p_resp->p_content_type_param[0]);
    const http_header_date_str_t date_str = http_server_gen_header_date_str(true);
    http_server_netconn_printf(
        p_conn,
        true,
        "HTTP/1.1 403 Forbidden\r\n"
        "Server: Ruuvi Gateway\r\n"
        "%s"
        "Content-type: %s; charset=utf-8%s%s\r\n"
        "Content-Length: %lu\r\n"
        "%s"
        "%s"
        "%s"
        "\r\n",
        date_str.buf,
        http_get_content_type_str(p_resp->content_type),
        use_extra_content_type_param ? "; " : "",
        use_extra_content_type_param ? p_resp->p_content_type_param : "",
        (printf_ulong_t)p_resp->content_len,
        (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : "",
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

static void
http_server_netconn_resp_404(struct netconn *p_conn)
{
    LOG_WARN("Respond: 404 Not Found");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 404 Not Found\r\n"
        "Server: Ruuvi Gateway\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

static void
http_server_netconn_resp_503(struct netconn *p_conn)
{
    LOG_WARN("Respond: 503 Service Unavailable");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 503 Service Unavailable\r\n"
        "Server: Ruuvi Gateway\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

static void
http_server_netconn_resp_504(struct netconn *p_conn)
{
    LOG_WARN("Respond: 504 Gateway timeout");
    http_server_netconn_printf(
        p_conn,
        false,
        "HTTP/1.1 504 Gateway timeout\r\n"
        "Server: Ruuvi Gateway\r\n"
        "Content-Length: 0\r\n"
        "\r\n");
}

void
http_server_start(void)
{
    LOG_INFO("Start HTTP-Server");
    assert(NULL != g_http_server_mutex);

    os_mutex_lock(g_http_server_mutex);
    if (os_signal_is_any_thread_registered(gp_http_server_sig))
    {
        LOG_WARN("Another HTTP-Server is already working");
        os_mutex_unlock(g_http_server_mutex);
    }
    os_mutex_unlock(g_http_server_mutex);

    const uint32_t stack_depth = 20U * 1024U;
    if (!os_task_create_finite_without_param(
            &http_server_task,
            "http_server",
            stack_depth,
            WIFI_MANAGER_TASK_PRIORITY - 1))
    {
        LOG_ERR("xTaskCreate failed: http_server");
    }
}

void
http_server_stop(void)
{
    assert(NULL != g_http_server_mutex);
    os_mutex_lock(g_http_server_mutex);
    if (os_signal_is_any_thread_registered(gp_http_server_sig))
    {
        LOG_INFO("Send request to stop HTTP-Server");
        if (!os_signal_send(gp_http_server_sig, HTTP_SERVER_SIG_STOP))
        {
            LOG_ERR("Failed to send HTTP-Server stop request");
        }
    }
    else
    {
        LOG_INFO("Send request to stop HTTP-Server, but HTTP-Server is not running");
    }
    os_mutex_unlock(g_http_server_mutex);
}

static bool
http_server_sig_register_cur_thread(void)
{
    os_mutex_lock(g_http_server_mutex);
    if (!os_signal_register_cur_thread(gp_http_server_sig))
    {
        os_mutex_unlock(g_http_server_mutex);
        return false;
    }
    os_mutex_unlock(g_http_server_mutex);
    return true;
}

static void
http_server_sig_unregister_cur_thread(void)
{
    os_mutex_lock(g_http_server_mutex);
    os_signal_unregister_cur_thread(gp_http_server_sig);
    os_mutex_unlock(g_http_server_mutex);
}

void
http_server_update_last_http_status_request(void)
{
    g_timestamp_last_http_status_request = xTaskGetTickCount();
}

static bool
http_server_is_status_json_timeout_expired(const uint32_t timeout_ms)
{
    if ((xTaskGetTickCount() - g_timestamp_last_http_status_request) > OS_DELTA_MS_TO_TICKS(timeout_ms))
    {
        return true;
    }
    return false;
}

void
http_server_on_ap_sta_connected(void)
{
    g_is_ap_sta_ip_assigned = false;
    http_server_update_last_http_status_request();
    LOG_DBG("http_server_on_ap_sta_connected: %lu", (printf_ulong_t)g_timestamp_last_http_status_request);
}

void
http_server_on_ap_sta_disconnected(void)
{
    g_is_ap_sta_ip_assigned = false;
    http_server_update_last_http_status_request();
    LOG_DBG("http_server_on_ap_sta_disconnected: %lu", (printf_ulong_t)g_timestamp_last_http_status_request);
}

void
http_server_on_ap_sta_ip_assigned(void)
{
    g_is_ap_sta_ip_assigned = true;
    http_server_update_last_http_status_request();
    LOG_DBG("http_server_on_ap_sta_ip_assigned: %lu", (printf_ulong_t)g_timestamp_last_http_status_request);
}

static bool
http_server_check_if_configuring_complete_wifi(
    bool *const            p_is_network_connected,
    bool *const            p_is_ap_sta_ip_assigned,
    const os_delta_ticks_t time_for_processing_request)
{
    if (!*p_is_network_connected)
    {
        *p_is_network_connected = true;
        http_server_update_last_http_status_request();
    }
    if ((!*p_is_ap_sta_ip_assigned) && wifi_manager_is_ap_sta_ip_assigned())
    {
        *p_is_ap_sta_ip_assigned = true;
        http_server_update_last_http_status_request();
    }
    else if ((*p_is_ap_sta_ip_assigned) && (!wifi_manager_is_ap_sta_ip_assigned()))
    {
        *p_is_ap_sta_ip_assigned = false;
        http_server_update_last_http_status_request();
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
    if (*p_is_ap_sta_ip_assigned)
    {
        const uint32_t timeout_ms = HTTP_SERVER_STATUS_JSON_REQUEST_TIMEOUT_MS;
        if ((!g_http_server_disable_ap_stopping_by_timeout)
            && http_server_is_status_json_timeout_expired(timeout_ms + time_for_processing_request))
        {
            LOG_INFO("There are no HTTP requests for status.json while AP_STA is connected for %u ms", timeout_ms);
            return true;
        }
    }
    else
    {
        const uint32_t timeout_ms = HTTP_SERVER_STA_AP_TIMEOUT_MS;
        if ((!g_http_server_disable_ap_stopping_by_timeout)
            && http_server_is_status_json_timeout_expired(timeout_ms + time_for_processing_request))
        {
            LOG_INFO("There are no HTTP requests for status.json while AP_STA is not connected for %u ms", timeout_ms);
            return true;
        }
    }
    return false;
}

static bool
http_server_check_if_configuring_complete_ethernet(
    bool *const            p_is_network_connected,
    const os_delta_ticks_t time_for_processing_request)
{
    if (!*p_is_network_connected)
    {
        *p_is_network_connected = true;
        http_server_update_last_http_status_request();
    }
    if ((!g_http_server_disable_ap_stopping_by_timeout)
        && http_server_is_status_json_timeout_expired(
            HTTP_SERVER_STATUS_JSON_REQUEST_TIMEOUT_MS + time_for_processing_request))
    {
        LOG_INFO("There are no HTTP requests for status.json for %u ms", HTTP_SERVER_STATUS_JSON_REQUEST_TIMEOUT_MS);
        return true;
    }
    return false;
}

static bool
http_server_check_if_configuring_complete(const os_delta_ticks_t time_for_processing_request)
{
    static bool g_is_network_connected = false;

    if (wifi_manager_is_ap_active() && wifi_manager_is_connected_to_wifi())
    {
        return http_server_check_if_configuring_complete_wifi(
            &g_is_network_connected,
            &g_is_ap_sta_ip_assigned,
            time_for_processing_request);
    }
    else if (wifi_manager_is_connected_to_ethernet())
    {
        http_server_check_if_configuring_complete_ethernet(&g_is_network_connected, time_for_processing_request);
    }
    else
    {
        g_is_network_connected  = false;
        g_is_ap_sta_ip_assigned = false;
    }
    return false;
}

static void
http_server_task(void)
{
    LOG_INFO("HTTP-Server thread started");
    if (!http_server_sig_register_cur_thread())
    {
        LOG_WARN("Another HTTP-Server is already working - finish current thread");
        return;
    }

    struct netconn *p_conn    = netconn_new(NETCONN_TCP);
    const u16_t     http_port = 80U;
    netconn_bind(p_conn, IP_ADDR_ANY, http_port);
    netconn_listen(p_conn);
    netconn_set_recvtimeout(p_conn, HTTP_SERVER_ACCEPT_TIMEOUT_MS);
    LOG_INFO("HTTP Server listening on 80/tcp");
    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (os_signal_wait_with_timeout(gp_http_server_sig, OS_DELTA_TICKS_IMMEDIATE, &sig_events))
        {
            LOG_INFO("Got signal STOP");
            break;
        }
        struct netconn *p_new_conn = NULL;

        const err_t err = netconn_accept(p_conn, &p_new_conn);

        os_delta_ticks_t time_for_processing_request = 0;
        if (ERR_OK == err)
        {
            if (NULL == p_new_conn)
            {
                LOG_ERR("netconn_accept returned OK, but p_new_conn is NULL");
            }
            else
            {
                const os_delta_ticks_t t0 = xTaskGetTickCount();
                http_server_netconn_serve(p_new_conn);
                netconn_close(p_new_conn);
                netconn_delete(p_new_conn);
                time_for_processing_request = xTaskGetTickCount() - t0;
            }
        }
        else if (ERR_TIMEOUT == err)
        {
            LOG_VERBOSE("netconn_accept ERR_TIMEOUT");
        }
        else if (ERR_ABRT == err)
        {
            LOG_ERR("netconn_accept ERR_ABRT");
        }
        else
        {
            LOG_ERR("netconn_accept: %d", err);
        }
        if (wifi_manager_is_ap_active() && http_server_check_if_configuring_complete(time_for_processing_request)
            && wifi_manager_is_connected_to_wifi_or_ethernet())
        {
            LOG_INFO("Stop WiFi AP");
            wifi_manager_stop_ap();
        }
        taskYIELD(); /* allows the freeRTOS scheduler to take over if needed. */
    }
    LOG_INFO("Stop HTTP-Server");
    netconn_close(p_conn);
    netconn_delete(p_conn);
    http_server_sig_unregister_cur_thread();
}

static const char *
get_http_body(const char *msg, uint32_t len, uint32_t *p_body_len)
{
    static const char g_newlines[] = "\r\n\r\n";

    const char *p_body = strstr(msg, g_newlines);
    if (NULL == p_body)
    {
        LOG_DBG("http body not found: %s", msg);
        return 0;
    }
    p_body += strlen(g_newlines);
    if (NULL != p_body_len)
    {
        *p_body_len = len - (uint32_t)(ptrdiff_t)(p_body - msg);
    }
    return p_body;
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
        LOG_WARN("fullbuf full, fullsize: %u, buflen: %d", *p_req_size, buflen);
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
            LOG_DBG("Header Content-Length: %d, HTTP body length: %d", content_len, body_len);
            if (content_len == body_len)
            {
                // HTTP request is full
                *p_req_ready = true;
            }
            else
            {
                LOG_DBG("request not full yet");
            }
        }
        else
        {
            LOG_DBG("Header Content-Length: %d, body not found", content_len);
            // read more data
        }
    }
    else
    {
        *p_req_ready = true;
    }
    return true;
}

static void
http_server_netconn_serve(struct netconn *const p_conn)
{
    char     req_buf[FULLBUF_SIZE + 1];
    uint32_t req_size  = 0;
    bool     req_ready = false;

    sta_ip_string_t local_ip_str  = { '\0' };
    sta_ip_string_t remote_ip_str = { '\0' };
    ipaddr_ntoa_r(&p_conn->pcb.tcp->local_ip, local_ip_str.buf, sizeof(local_ip_str.buf));
    ipaddr_ntoa_r(&p_conn->pcb.tcp->remote_ip, remote_ip_str.buf, sizeof(remote_ip_str.buf));

    while (!req_ready)
    {
        if (!http_server_recv_and_handle(p_conn, &req_buf[0], &req_size, &req_ready))
        {
            break;
        }
    }

    if (!req_ready)
    {
        LOG_WARN("the connection was closed by the client side");
        return;
    }
    LOG_DBG("Request from %s to %s: %s", remote_ip_str.buf, local_ip_str.buf, req_buf);

    const http_req_info_t req_info = http_req_parse(req_buf);

    LOG_INFO(
        "Request from %s to %s: %s %s",
        remote_ip_str.buf,
        local_ip_str.buf,
        req_info.http_cmd.ptr,
        req_info.http_uri.ptr);

    LOG_DBG("p_http_cmd: %s", req_info.http_cmd.ptr);
    LOG_DBG("p_http_uri: %s", req_info.http_uri.ptr);
    LOG_DBG("p_http_ver: %s", req_info.http_ver.ptr);
    LOG_DBG("p_http_header: %s", req_info.http_header.ptr);
    LOG_DBG("p_http_body: %s", req_info.http_body.ptr);

    if (!req_info.is_success)
    {
        http_server_netconn_resp_400(p_conn);
        return;
    }
    const bool is_wifi_manager_working = wifi_manager_is_working();

    /* captive portal functionality: redirect to access point IP for HOST that are not the access point IP OR the
     * STA IP */
    uint32_t    host_len = 0;
    const char *p_host   = http_req_header_get_field(req_info.http_header, "Host:", &host_len);

    /* determine if Host is from the STA IP address */
    const sta_ip_string_t ip_str = sta_ip_safe_get();

    const bool is_access_to_sta_ip = ('\0' != ip_str.buf[0]) && (host_len > 0) && (NULL != strstr(p_host, ip_str.buf));
    const bool is_request_to_ap_ip = ((host_len > 0) && (NULL != strstr(p_host, DEFAULT_AP_IP)));

    LOG_DBG("Host: %.*s, StaticIP: %s", host_len, p_host, ip_str.buf);

    if (is_wifi_manager_working && (!is_request_to_ap_ip) && (!is_access_to_sta_ip))
    {
        http_server_netconn_resp_302(p_conn);
        return;
    }

    const bool flag_access_from_lan = (0 != strcmp(local_ip_str.buf, DEFAULT_AP_IP)) ? true : false;

    g_http_server_extra_header_fields.buf[0] = '\0';

    http_server_resp_t resp = http_server_handle_req(
        &req_info,
        &remote_ip_str,
        http_server_get_auth(),
        &g_http_server_extra_header_fields,
        flag_access_from_lan);
    if ((HTTP_CONENT_TYPE_APPLICATION_JSON == resp.content_type)
        && ((HTTP_CONTENT_LOCATION_STATIC_MEM == resp.content_location)
            || (HTTP_CONTENT_LOCATION_HEAP == resp.content_location)))
    {
        LOG_INFO("Json resp: code=%u, content:\n%s", resp.http_resp_code, resp.select_location.memory.p_buf);
    }
    switch (resp.http_resp_code)
    {
        case HTTP_RESP_CODE_200:
            http_server_netconn_resp_200(p_conn, &resp);
            return;
        case HTTP_RESP_CODE_302:
            http_server_netconn_resp_302_auth_html(p_conn, &local_ip_str);
            return;
        case HTTP_RESP_CODE_400:
            http_server_netconn_resp_400(p_conn);
            return;
        case HTTP_RESP_CODE_401:
            http_server_netconn_resp_401(p_conn, &resp, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_403:
            http_server_netconn_resp_403(p_conn, &resp, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_404:
            http_server_netconn_resp_404(p_conn);
            return;
        case HTTP_RESP_CODE_503:
            http_server_netconn_resp_503(p_conn);
            return;
        case HTTP_RESP_CODE_504:
            http_server_netconn_resp_504(p_conn);
            return;
    }
    assert(0);
    http_server_netconn_resp_503(p_conn);
}
