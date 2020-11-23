/*
Copyright (c) 2019 Tony Pottier

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

@file dns_server.c
@author Tony Pottier
@brief Defines an extremely basic DNS server for captive portal functionality.
It's basically a DNS hijack that replies to the esp's address no matter which
request is sent to it.

Contains the freeRTOS task for the DNS server that processes the requests.

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <lwip/sockets.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_system.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_err.h>
#include <nvs_flash.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <lwip/dns.h>
#include <byteswap.h>
#include "wifi_manager.h"
#include "dns_server.h"
#include "attribs.h"
#include "os_task.h"
#include "log.h"
#include "esp_type_wrapper.h"

static const char TAG[] = "dns_server";

static TaskHandle_t gh_dns_task;
static socket_t     g_dns_socket_fd = SOCKET_INVALID;

ATTR_NORETURN
void
dns_server_task(ATTR_UNUSED void *p_params);

bool
dns_server_start(void)
{
    /* Create UDP socket */
    socket_t socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (SOCKET_INVALID == socket_fd)
    {
        LOG_ERR("Failed to create socket 53/udp");
        return false;
    }
    struct sockaddr_in ra = { 0 };
    /* Bind to port 53 (typical DNS Server port) */
    tcpip_adapter_ip_info_t ip = { 0 };
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip);
    ra.sin_family      = AF_INET;
    ra.sin_addr.s_addr = ip.ip.addr;
    ra.sin_port        = htons(53);
    if (SOCKET_BIND_ERROR == bind(socket_fd, (struct sockaddr *)&ra, sizeof(struct sockaddr_in)))
    {
        LOG_ERR("Failed to bind to 53/udp");
        close(socket_fd);
        return false;
    }

    g_dns_socket_fd = socket_fd;

    const uint32_t stack_depth = 3072U;
    if (!os_task_create(
            &dns_server_task,
            "dns_server",
            stack_depth,
            NULL,
            WIFI_MANAGER_TASK_PRIORITY - 1,
            &gh_dns_task))
    {
        LOG_ERR("Can't create thread");
        return false;
    }
    return true;
}

void
dns_server_stop(void)
{
    if (NULL != gh_dns_task)
    {
        vTaskDelete(gh_dns_task);
        close(g_dns_socket_fd);
        g_dns_socket_fd = SOCKET_INVALID;
        gh_dns_task     = NULL;
    }
}

static void
replace_non_ascii_with_dots(char *p_domain)
{
    for (char *p_ch = p_domain; *p_ch != '\0'; ++p_ch)
    {
        if ((*p_ch < ' ') || (*p_ch > 'z'))
        {
            /* Technically we should test if the first two bits are 00 (e.g. '0x00 == (*p_ch & 0xC0)')
             * but this makes the code a lot more readable */
            *p_ch = '.';
        }
    }
}

static void
dns_server_handle_req(socket_t socket_fd, const ip4_addr_t *p_ip_resolved)
{
    struct sockaddr_in client     = { 0 };
    socklen_t          client_len = sizeof(client);
    uint8_t            data[DNS_QUERY_MAX_SIZE + 1];  /* dns query buffer */
    uint8_t            response[DNS_ANSWER_MAX_SIZE]; /* dns response buffer */
    char ip_address[INET_ADDRSTRLEN]; /* buffer to store IPs as text. This is only used for debug and serves no other
                                         purpose */

    memset(data, 0x00, sizeof(data));
    const socket_recv_result_t length
        = recvfrom(socket_fd, data, DNS_QUERY_MAX_SIZE, 0, (struct sockaddr *)&client, &client_len);

    /*if the query is bigger than the buffer size we simply ignore it. This case should only happen in case of
     * multiple queries within the same DNS packet and is not supported by this simple DNS hijack. */
    if (length < 0)
    {
        LOG_ERR("recvfrom got length less than 0");
        return;
    }
    if ((size_t)length < sizeof(dns_header_t))
    {
        LOG_ERR("recvfrom got length less than dsn_header (%u bytes)", (uint32_t)sizeof(dns_header_t));
        return;
    }
    const size_t max_allowed_len = DNS_ANSWER_MAX_SIZE - sizeof(dns_answer_t);
    if ((size_t)length > max_allowed_len)
    {
        LOG_ERR("recvfrom got length %d > max allowed %u", length, (uint32_t)max_allowed_len);
        return;
    }
    data[length] = '\0'; /*in case there's a bogus domain name that isn't null terminated */

    /* Generate header message */
    memcpy(response, data, sizeof(dns_header_t));
    dns_header_t *dns_header              = (dns_header_t *)response;
    dns_header->flag_query_response       = 1;                          /*response bit */
    dns_header->operation_code            = DNS_OPCODE_QUERY;           /* no support for other type of response */
    dns_header->flag_authoritative_answer = 1;                          /*authoritative answer */
    dns_header->response_ode              = DNS_REPLY_CODE_NO_ERROR;    /* no error */
    dns_header->flag_truncation           = 0;                          /*no truncation */
    dns_header->flag_recursion_desired    = 0;                          /*no recursion */
    dns_header->answer_record_count       = dns_header->question_count; /* set answer count = question count -- duhh! */
    dns_header->authority_record_count    = 0x0000;                     /* name server resource records = 0 */
    dns_header->additional_record_count   = 0x0000;                     /* resource records = 0 */

    /* copy the rest of the query in the response */
    memcpy(response + sizeof(dns_header_t), data + sizeof(dns_header_t), length - sizeof(dns_header_t));

    /* extract domain name and request IP for debug */
    inet_ntop(AF_INET, &(client.sin_addr), ip_address, INET_ADDRSTRLEN);
    char *p_domain = (char *)&data[sizeof(dns_header_t) + 1];
    replace_non_ascii_with_dots(p_domain);
    LOG_INFO("Replying to DNS request for %s from %s", p_domain, ip_address);

    /* create DNS answer at the end of the query*/
    dns_answer_t *p_dns_answer = (dns_answer_t *)&response[length];
    p_dns_answer->domain_name  = htons(
        0xC00C); /* This is a pointer to the beginning of the question.
                   * As per DNS standard, first two bits must be set to 11 for some odd reason hence 0xC0 */
    p_dns_answer->dns_response_type  = htons(DNS_ANSWER_TYPE_A);
    p_dns_answer->dns_response_class = htons(DNS_ANSWER_CLASS_IN);
    p_dns_answer->time_to_live_seconds
        = (uint32_t)0x00000000; /* no caching. Avoids DNS poisoning since this is a DNS hijack */
    p_dns_answer->dns_response_data_length = htons(0x0004); /* 4 byte => size of an ipv4 address */
    p_dns_answer->dns_response_data        = p_ip_resolved->addr;

    const socket_send_result_t err
        = sendto(socket_fd, response, length + sizeof(dns_answer_t), 0, (struct sockaddr *)&client, client_len);
    if (err < 0)
    {
        LOG_ERR("UDP sendto failed: %d", err);
    }
}

ATTR_NORETURN
void
dns_server_task(ATTR_UNUSED void *p_params)
{
    const socket_t socket_fd = g_dns_socket_fd;

    /* Set redirection DNS hijack to the access point IP */
    ip4_addr_t ip_resolved = { 0 };
    inet_pton(AF_INET, DEFAULT_AP_IP, &ip_resolved);

    LOG_INFO("DNS Server listening on 53/udp");

    /* Start loop to process DNS requests */
    for (;;)
    {
        dns_server_handle_req(socket_fd, &ip_resolved);

        taskYIELD(); /* allows the freeRTOS scheduler to take over if needed. DNS daemon should not be taxing on the
                        system */
    }
    close(g_dns_socket_fd);
    g_dns_socket_fd = SOCKET_INVALID;

    vTaskDelete(NULL);
}
