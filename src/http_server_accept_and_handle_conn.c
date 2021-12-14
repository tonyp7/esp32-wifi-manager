/**
 * @file http_server_accept_and_handle_conn.c
 * @author TheSomeMan
 * @date 2021-11-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_accept_and_handle_conn.h"
#include "lwip/priv/tcp_priv.h"
#include "os_sema.h"
#include "os_malloc.h"
#include "str_buf.h"
#include "wifi_manager.h"
#include "sta_ip.h"
#include "http_req.h"
#include "sta_ip_safe.h"
#include "http_server_auth.h"
#include "http_server_handle_req.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define FULLBUF_SIZE (4U * 1024U)

#define HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG (256U)

#define HTTP_HEADER_DATE_EXAMPLE "Date: Thu, 01 Jan 2021 00:00:00 GMT\r\n"

typedef struct http_header_date_str_t
{
    char buf[sizeof(HTTP_HEADER_DATE_EXAMPLE)];
} http_header_date_str_t;

static const char TAG[] = "http_server";

static http_header_extra_fields_t g_http_server_extra_header_fields;

static const char *
get_http_body(const char *const p_msg, const uint32_t len, uint32_t *const p_body_len)
{
    static const char g_newlines[] = "\r\n\r\n";

    const char *p_body = strstr(p_msg, g_newlines);
    if (NULL == p_body)
    {
        LOG_DBG("http body not found: %s", p_msg);
        return 0;
    }
    p_body += strlen(g_newlines);
    if (NULL != p_body_len)
    {
        *p_body_len = len - (uint32_t)(ptrdiff_t)(p_body - p_msg);
    }
    return p_body;
}

static bool
http_server_recv_and_handle(
    struct netconn *const p_conn,
    char *const           p_req_buf,
    uint32_t *const       p_req_size,
    bool *const           p_req_ready)
{
    struct netbuf *p_netbuf_in = NULL;

    const os_delta_ticks_t t0                    = xTaskGetTickCount();
    const err_t            err                   = netconn_recv(p_conn, &p_netbuf_in);
    const os_delta_ticks_t time_for_netconn_recv = xTaskGetTickCount() - t0;
    if (ERR_OK != err)
    {
        LOG_ERR("netconn recv: %d (time: %lu ticks)", (printf_int_t)err, (printf_ulong_t)time_for_netconn_recv);
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

static bool
http_server_netconn_write(
    struct netconn *const p_conn,
    const void *const     p_buf,
    const size_t          buf_len,
    const uint8_t         netconn_flags)
{
    /**
     * It's not enough to just set timeout with netconn_set_sendtimeout because if the WiFi connection is lost,
     * then netconn_write_partly will ignore p_conn->send_timeout and will wait much longer,
     * which will trigger task watchdog for http_server.
     */
    size_t offset = 0;
    do
    {
        size_t bytes_written = 0;

        http_server_sema_send_wait_immediate();
        const err_t err = netconn_write_partly(
            p_conn,
            &((const uint8_t *)p_buf)[offset],
            buf_len - offset,
            netconn_flags | (uint8_t)NETCONN_DONTBLOCK,
            &bytes_written);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "netconn_write_partly failed");
            return false;
        }
        offset += bytes_written;
        if (!http_server_sema_send_wait_timeout(p_conn->send_timeout))
        {
            LOG_ERR("netconn_write_partly failed: send timeout");
            return false;
        }
    } while (offset != buf_len);
    return true;
}

ATTR_PRINTF(3, 4)
static bool
http_server_netconn_printf(struct netconn *const p_conn, const bool flag_more, const char *const p_fmt, ...)
{
    va_list args;
    va_start(args, p_fmt);
    str_buf_t str_buf = str_buf_vprintf_with_alloc(p_fmt, args);
    va_end(args);
    if (NULL == str_buf.buf)
    {
        LOG_ERR("Can't allocate memory for buffer");
        return false;
    }
    LOG_DBG("Respond: %s", str_buf.buf);
    uint8_t netconn_flags = (uint8_t)NETCONN_COPY;
    if (flag_more)
    {
        netconn_flags |= (uint8_t)NETCONN_MORE;
    }
    LOG_DBG("netconn_write: %u bytes", (printf_uint_t)str_buf_get_len(&str_buf));

    const bool res = http_server_netconn_write(p_conn, str_buf.buf, str_buf_get_len(&str_buf), netconn_flags);

    str_buf_free_buf(&str_buf);
    if (!res)
    {
        LOG_ERR("%s failed", "http_server_netconn_write");
        return false;
    }
    return true;
}

static const char *
http_get_content_type_str(const http_content_type_e content_type)
{
    const char *p_content_type_str = "application/octet-stream";
    switch (content_type)
    {
        case HTTP_CONENT_TYPE_TEXT_HTML:
            p_content_type_str = "text/html";
            break;
        case HTTP_CONENT_TYPE_TEXT_PLAIN:
            p_content_type_str = "text/plain";
            break;
        case HTTP_CONENT_TYPE_TEXT_CSS:
            p_content_type_str = "text/css";
            break;
        case HTTP_CONENT_TYPE_TEXT_JAVASCRIPT:
            p_content_type_str = "text/javascript";
            break;
        case HTTP_CONENT_TYPE_IMAGE_PNG:
            p_content_type_str = "image/png";
            break;
        case HTTP_CONENT_TYPE_IMAGE_SVG_XML:
            p_content_type_str = "image/svg+xml";
            break;
        case HTTP_CONENT_TYPE_APPLICATION_JSON:
            p_content_type_str = "application/json";
            break;
        case HTTP_CONENT_TYPE_APPLICATION_OCTET_STREAM:
            p_content_type_str = "application/octet-stream";
            break;
    }
    return p_content_type_str;
}

const char *
http_get_content_encoding_str(const http_server_resp_t *const p_resp)
{
    const char *p_content_encoding_str = "";
    switch (p_resp->content_encoding)
    {
        case HTTP_CONENT_ENCODING_NONE:
            p_content_encoding_str = "";
            break;
        case HTTP_CONENT_ENCODING_GZIP:
            p_content_encoding_str = "Content-Encoding: gzip\r\n";
            break;
    }
    return p_content_encoding_str;
}

static const char *
http_get_cache_control_str(const http_server_resp_t *const p_resp)
{
    const char *p_cache_control_str = "";
    if (p_resp->flag_no_cache)
    {
        p_cache_control_str
            = "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
              "Pragma: no-cache\r\n";
    }
    return p_cache_control_str;
}

static void
write_content_from_memory(struct netconn *const p_conn, const http_server_resp_t *const p_resp)
{
    LOG_DBG("netconn_write: %u bytes", p_resp->content_len);
    const bool res = http_server_netconn_write(
        p_conn,
        p_resp->select_location.memory.p_buf,
        p_resp->content_len,
        NETCONN_NOCOPY);
    if (!res)
    {
        LOG_ERR("%s failed", "http_server_netconn_write");
    }
}

static void
write_content_from_heap(struct netconn *const p_conn, http_server_resp_t *const p_resp)
{
    write_content_from_memory(p_conn, p_resp);
    os_free(p_resp->select_location.memory.p_buf);
}

static void
write_content_from_fatfs(struct netconn *const p_conn, const http_server_resp_t *const p_resp)
{
    const size_t tmp_buf_size = FULLBUF_SIZE;
    char *       p_tmp_buf    = os_malloc(tmp_buf_size);
    if (NULL == p_tmp_buf)
    {
        LOG_ERR("Can't allocate memory for temporary buffer");
        return;
    }
    uint32_t rem_len = p_resp->content_len;
    while (rem_len > 0)
    {
        const uint32_t num_bytes       = (rem_len <= tmp_buf_size) ? rem_len : tmp_buf_size;
        const bool     flag_last_block = (num_bytes == rem_len) ? true : false;

        const file_read_result_t read_result = read(p_resp->select_location.fatfs.fd, p_tmp_buf, num_bytes);
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
        LOG_DBG("netconn_write: %u bytes", num_bytes);
        const bool res = http_server_netconn_write(p_conn, p_tmp_buf, num_bytes, netconn_flags);
        if (!res)
        {
            LOG_ERR("%s failed", "http_server_netconn_write");
            break;
        }
    }
    os_free(p_tmp_buf);
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
http_server_write_content(struct netconn *const p_conn, http_server_resp_t *const p_resp)
{
    switch (p_resp->content_location)
    {
        case HTTP_CONTENT_LOCATION_NO_CONTENT:
            break;
        case HTTP_CONTENT_LOCATION_FLASH_MEM:
            ATTR_FALLTHROUGH;
        case HTTP_CONTENT_LOCATION_STATIC_MEM:
            write_content_from_memory(p_conn, p_resp);
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
http_server_netconn_resp_with_content(
    struct netconn *const                   p_conn,
    http_server_resp_t *const               p_resp,
    const http_header_extra_fields_t *const p_extra_header_fields,
    const http_resp_code_e                  resp_code,
    const char *const                       p_status_msg)
{
    if (HTTP_RESP_CODE_200 == resp_code)
    {
        LOG_INFO("Respond: %s", p_status_msg);
    }
    else
    {
        LOG_WARN("Respond: %s", p_status_msg);
    }
    const bool use_extra_content_type_param = (NULL != p_resp->p_content_type_param)
                                              && ('\0' != p_resp->p_content_type_param[0]);
    const http_header_date_str_t date_str = http_server_gen_header_date_str(true);
    if (!http_server_netconn_printf(
            p_conn,
            true,
            "HTTP/1.1 %u %s\r\n"
            "Server: Ruuvi Gateway\r\n"
            "%s"
            "Content-type: %s; charset=utf-8%s%s\r\n"
            "Content-Length: %lu\r\n"
            "%s"
            "%s"
            "%s"
            "\r\n",
            (printf_uint_t)resp_code,
            p_status_msg,
            date_str.buf,
            http_get_content_type_str(p_resp->content_type),
            use_extra_content_type_param ? "; " : "",
            use_extra_content_type_param ? p_resp->p_content_type_param : "",
            (printf_ulong_t)p_resp->content_len,
            (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : "",
            http_get_content_encoding_str(p_resp),
            http_get_cache_control_str(p_resp)))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }

    http_server_write_content(p_conn, p_resp);
}

static void
http_server_netconn_resp_without_content(
    struct netconn *const  p_conn,
    const http_resp_code_e resp_code,
    const char *const      p_status_msg)
{
    LOG_WARN("Respond: %s", p_status_msg);
    if (!http_server_netconn_printf(
            p_conn,
            false,
            "HTTP/1.1 %u %s\r\n"
            "Server: Ruuvi Gateway\r\n"
            "Content-Length: 0\r\n"
            "\r\n",
            (printf_uint_t)resp_code,
            p_status_msg))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }
}

static void
http_server_netconn_resp_200(
    struct netconn *const                   p_conn,
    http_server_resp_t *const               p_resp,
    const http_header_extra_fields_t *const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_200, "OK");
}

static void
http_server_netconn_resp_302(struct netconn *const p_conn)
{
    LOG_INFO("Respond: 302 Found");
    if (!http_server_netconn_printf(
            p_conn,
            false,
            "HTTP/1.1 302 Found\r\n"
            "Server: Ruuvi Gateway\r\n"
            "Location: http://%s/\r\n"
            "\r\n",
            DEFAULT_AP_IP))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }
}

static void
http_server_netconn_resp_302_auth_html(
    struct netconn *const                   p_conn,
    const sta_ip_string_t *const            p_ip_str,
    const http_header_extra_fields_t *const p_extra_header_fields)
{
    LOG_INFO("Respond: 302 Found");
    if (!http_server_netconn_printf(
            p_conn,
            false,
            "HTTP/1.1 302 Found\r\n"
            "Server: Ruuvi Gateway\r\n"
            "Location: http://%s/auth.html\r\n"
            "%s"
            "\r\n",
            p_ip_str->buf,
            (NULL != p_extra_header_fields) ? p_extra_header_fields->buf : ""))
    {
        LOG_ERR("%s failed", "http_server_netconn_printf");
        return;
    }
}

static void
http_server_netconn_resp_400(struct netconn *const p_conn)
{
    http_server_netconn_resp_without_content(p_conn, HTTP_RESP_CODE_400, "Bad Request");
}

static void
http_server_netconn_resp_401(
    struct netconn *const                   p_conn,
    http_server_resp_t *const               p_resp,
    const http_header_extra_fields_t *const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_401, "Unauthorized");
}

static void
http_server_netconn_resp_403(
    struct netconn *const                   p_conn,
    http_server_resp_t *const               p_resp,
    const http_header_extra_fields_t *const p_extra_header_fields)
{
    http_server_netconn_resp_with_content(p_conn, p_resp, p_extra_header_fields, HTTP_RESP_CODE_403, "Forbidden");
}

static void
http_server_netconn_resp_404(struct netconn *const p_conn)
{
    http_server_netconn_resp_without_content(p_conn, HTTP_RESP_CODE_404, "Not Found");
}

static void
http_server_netconn_resp_503(struct netconn *const p_conn)
{
    http_server_netconn_resp_without_content(p_conn, HTTP_RESP_CODE_503, "Service Unavailable");
}

static void
http_server_netconn_resp_504(struct netconn *const p_conn)
{
    http_server_netconn_resp_without_content(p_conn, HTTP_RESP_CODE_504, "Gateway timeout");
}

static void
http_server_netconn_resp(
    struct netconn *const        p_conn,
    http_server_resp_t *const    p_resp,
    const sta_ip_string_t *const p_local_ip_str)
{
    switch (p_resp->http_resp_code)
    {
        case HTTP_RESP_CODE_200:
            http_server_netconn_resp_200(p_conn, p_resp, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_302:
            http_server_netconn_resp_302_auth_html(p_conn, p_local_ip_str, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_400:
            http_server_netconn_resp_400(p_conn);
            return;
        case HTTP_RESP_CODE_401:
            http_server_netconn_resp_401(p_conn, p_resp, &g_http_server_extra_header_fields);
            return;
        case HTTP_RESP_CODE_403:
            http_server_netconn_resp_403(p_conn, p_resp, &g_http_server_extra_header_fields);
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

/**
 * @brief Helper function that processes one HTTP request at a time.
 * @param p_conn - ptr to a connection object
 */
static void
http_server_netconn_serve(struct netconn *const p_conn)
{
    char     req_buf[FULLBUF_SIZE + 1];
    uint32_t req_size  = 0;
    bool     req_ready = false;

    sta_ip_string_t local_ip_str  = { '\0' };
    sta_ip_string_t remote_ip_str = { '\0' };

    const struct tcp_pcb *const p_tcp = p_conn->pcb.tcp;
    if (NULL == p_tcp)
    {
        LOG_ERR("p_conn->pcb.tcp is NULL due to race condition(1)");
        return;
    }

    const ip_addr_t local_ip  = p_tcp->local_ip;
    const ip_addr_t remote_ip = p_tcp->remote_ip;

    if (NULL == p_conn->pcb.tcp)
    {
        LOG_ERR("p_conn->pcb.tcp is NULL due to race condition(2)");
        return;
    }

    ipaddr_ntoa_r(&local_ip, local_ip_str.buf, sizeof(local_ip_str.buf));
    ipaddr_ntoa_r(&remote_ip, remote_ip_str.buf, sizeof(remote_ip_str.buf));

    while (!req_ready)
    {
        if (!http_server_recv_and_handle(p_conn, &req_buf[0], &req_size, &req_ready))
        {
            break;
        }
    }

    if (!req_ready)
    {
        LOG_WARN("The connection was closed by the client side");
        return;
    }
    LOG_DBG("Request from %s to %s: %s", remote_ip_str.buf, local_ip_str.buf, req_buf);

    const http_req_info_t req_info = http_req_parse(req_buf);

    if (!req_info.is_success)
    {
        LOG_ERR("Request from %s to %s: failed to parse request: %s", remote_ip_str.buf, local_ip_str.buf, req_buf);
        http_server_netconn_resp_400(p_conn);
        return;
    }

    LOG_INFO(
        "Request from %s to %s: %s %s",
        remote_ip_str.buf,
        local_ip_str.buf,
        req_info.http_cmd.ptr ? req_info.http_cmd.ptr : "NULL",
        req_info.http_uri.ptr ? req_info.http_uri.ptr : "NULL");

    LOG_DBG("p_http_cmd: %s", req_info.http_cmd.ptr ? req_info.http_cmd.ptr : "NULL");
    LOG_DBG("p_http_uri: %s", req_info.http_uri.ptr ? req_info.http_uri.ptr : "NULL");
    LOG_DBG("p_http_ver: %s", req_info.http_ver.ptr ? req_info.http_ver.ptr : "NULL");
    LOG_DBG("p_http_header: %s", req_info.http_header.ptr ? req_info.http_header.ptr : "NULL");
    LOG_DBG("p_http_body: %s", req_info.http_body.ptr ? req_info.http_body.ptr : "NULL");

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
    if ('\0' != g_http_server_extra_header_fields.buf[0])
    {
        LOG_INFO("Extra HTTP-header resp: %s", g_http_server_extra_header_fields.buf);
    }
    if ((HTTP_CONENT_TYPE_APPLICATION_JSON == resp.content_type)
        && ((HTTP_CONTENT_LOCATION_STATIC_MEM == resp.content_location)
            || (HTTP_CONTENT_LOCATION_HEAP == resp.content_location)))
    {
        const size_t content_len = strlen((const char *)resp.select_location.memory.p_buf);
        if (content_len <= HTTP_SERVER_MAX_CONTENT_LEN_TO_PRINT_LOG)
        {
            LOG_INFO("Json resp: code=%u, content:\n%s", resp.http_resp_code, resp.select_location.memory.p_buf);
        }
        else
        {
            LOG_INFO("Json resp: code=%u, content_len=%lu", resp.http_resp_code, (printf_ulong_t)content_len);
        }
    }
    http_server_netconn_resp(p_conn, &resp, &local_ip_str);
}

os_delta_ticks_t
http_server_accept_and_handle_conn(struct netconn *const p_conn)
{
    struct netconn *p_new_conn = NULL;

    const err_t err = netconn_accept(p_conn, &p_new_conn);

    os_delta_ticks_t time_for_processing_request = 0;
    if (ERR_OK == err)
    {
        if (NULL == p_new_conn)
        {
            LOG_ERR("netconn_accept returned OK, but p_new_conn is NULL");
        }
        else if (NULL == p_conn->pcb.tcp)
        {
            // It seems that's a bug in netconn_accept, err is ERR_OK, p_new_conn is not NULL,
            // but p_conn->pcb.tcp is NULL.
            // Perhaps, err_tcp() was called. So the socked has already been closed.
            // As a workaround try to free resources and ignore this error.
            LOG_ERR("netconn_accept returned OK, but p_conn->pcb.tcp is NULL");
            netconn_delete(p_new_conn);
        }
        else
        {
            const int_fast32_t timeout_ms = 1500;
            netconn_set_recvtimeout(p_new_conn, timeout_ms);
            netconn_set_sendtimeout(p_new_conn, timeout_ms);
            const os_delta_ticks_t t0 = xTaskGetTickCount();
            LOG_DBG("call http_server_netconn_serve");
            http_server_netconn_serve(p_new_conn);
            LOG_DBG("call netconn_close");
            const err_t err_close = netconn_close(p_new_conn);
            if (ESP_OK != err_close)
            {
                LOG_ERR_ESP(err_close, "%s failed", "netconn_close");
            }
            LOG_DBG("call netconn_delete");
            const err_t err_delete = netconn_delete(p_new_conn);
            if (ESP_OK != err_delete)
            {
                LOG_ERR_ESP(err_delete, "%s failed", "netconn_delete");
            }
            time_for_processing_request = xTaskGetTickCount() - t0;
            LOG_DBG("req processed for %u ticks", (printf_uint_t)time_for_processing_request);
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
    return time_for_processing_request;
}
