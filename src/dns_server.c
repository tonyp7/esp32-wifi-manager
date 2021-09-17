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
#include <esp_task_wdt.h>
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
#include "os_signal.h"
#include "os_mutex.h"
#include "os_timer_sig.h"

typedef enum dns_server_sig_e
{
    DNS_SERVER_SIG_STOP               = OS_SIGNAL_NUM_0,
    DNS_SERVER_SIG_TASK_WATCHDOG_FEED = OS_SIGNAL_NUM_1,
} dns_server_sig_e;

#define DNS_SERVER_SIG_FIRST (DNS_SERVER_SIG_STOP)
#define DNS_SERVER_SIG_LAST  (DNS_SERVER_SIG_TASK_WATCHDOG_FEED)

static const char TAG[] = "dns_server";

static os_mutex_static_t              g_dns_server_mutex_mem;
static os_mutex_t                     g_dns_server_mutex;
static os_signal_static_t             g_dns_server_signal_mem;
static os_signal_t *                  g_p_dns_server_sig;
static os_timer_sig_periodic_t *      g_p_dns_server_timer_sig_watchdog_feed;
static os_timer_sig_periodic_static_t g_dns_server_timer_sig_watchdog_feed_mem;

ATTR_PURE
static os_signal_num_e
dns_server_conv_to_sig_num(const dns_server_sig_e sig)
{
    return (os_signal_num_e)sig;
}

static dns_server_sig_e
dns_server_conv_from_sig_num(const os_signal_num_e sig_num)
{
    assert(((os_signal_num_e)DNS_SERVER_SIG_FIRST <= sig_num) && (sig_num <= (os_signal_num_e)DNS_SERVER_SIG_LAST));
    return (dns_server_sig_e)sig_num;
}

static void
dns_server_task(void);

void
dns_server_init(void)
{
    assert(NULL == g_dns_server_mutex);
    g_dns_server_mutex = os_mutex_create_static(&g_dns_server_mutex_mem);
    g_p_dns_server_sig = os_signal_create_static(&g_dns_server_signal_mem);
    os_signal_add(g_p_dns_server_sig, dns_server_conv_to_sig_num(DNS_SERVER_SIG_STOP));
    os_signal_add(g_p_dns_server_sig, dns_server_conv_to_sig_num(DNS_SERVER_SIG_TASK_WATCHDOG_FEED));
}

bool
dns_server_start(void)
{
    LOG_INFO("Start DNS-Server");
    assert(NULL != g_dns_server_mutex);

    const uint32_t stack_depth = 3072U;
    if (!os_task_create_finite_without_param(
            &dns_server_task,
            "dns_server",
            stack_depth,
            WIFI_MANAGER_TASK_PRIORITY - 1))
    {
        LOG_ERR("Can't create thread");
        return false;
    }
    return true;
}

void
dns_server_stop(void)
{
    assert(NULL != g_dns_server_mutex);

    os_mutex_lock(g_dns_server_mutex);
    if (os_signal_is_any_thread_registered(g_p_dns_server_sig))
    {
        LOG_INFO("Send request to stop DNS-Server");
        if (!os_signal_send(g_p_dns_server_sig, DNS_SERVER_SIG_STOP))
        {
            LOG_ERR("Failed to send DNS-Server stop request");
        }
    }
    else
    {
        LOG_INFO("Send request to stop DNS-Server, but DSN-Server is not running");
    }
    os_mutex_unlock(g_dns_server_mutex);
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
        // timeout
        LOG_VERBOSE("recvfrom got length less than 0");
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

static bool
dns_server_sig_register_cur_thread(void)
{
    os_mutex_lock(g_dns_server_mutex);
    if (!os_signal_register_cur_thread(g_p_dns_server_sig))
    {
        os_mutex_unlock(g_dns_server_mutex);
        return false;
    }
    os_mutex_unlock(g_dns_server_mutex);
    return true;
}

static void
dns_server_sig_unregister_cur_thread(void)
{
    os_mutex_lock(g_dns_server_mutex);
    os_signal_unregister_cur_thread(g_p_dns_server_sig);
    os_mutex_unlock(g_dns_server_mutex);
}

static void
dns_server_wdt_add_and_start(void)
{
    LOG_INFO("TaskWatchdog: Register current thread");
    const esp_err_t err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_add");
    }
    LOG_INFO("TaskWatchdog: Start timer");
    os_timer_sig_periodic_start(g_p_dns_server_timer_sig_watchdog_feed);
}

static void
dns_server_task(void)
{
    LOG_INFO("DNS-Server thread started");
    if (!dns_server_sig_register_cur_thread())
    {
        LOG_WARN("Another DNS-Server is already working - finish current thread");
        return;
    }

    /* Create UDP socket */
    const socket_t socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (SOCKET_INVALID == socket_fd)
    {
        LOG_ERR("Failed to create socket 53/udp");
        dns_server_sig_unregister_cur_thread();
        return;
    }

    struct timeval tv = { .tv_sec = 1, .tv_usec = 0 };
    if (setsockopt(socket_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
    {
        LOG_ERR("setsockopt(SO_RCVTIMEO) failed");
        dns_server_sig_unregister_cur_thread();
        return;
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
        dns_server_sig_unregister_cur_thread();
        return;
    }

    /* Set redirection DNS hijack to the access point IP */
    ip4_addr_t ip_resolved = { 0 };
    inet_pton(AF_INET, DEFAULT_AP_IP, &ip_resolved);

    LOG_INFO("DNS Server listening on 53/udp");

    g_p_dns_server_timer_sig_watchdog_feed = os_timer_sig_periodic_create_static(
        &g_dns_server_timer_sig_watchdog_feed_mem,
        "dns:wdog",
        g_p_dns_server_sig,
        dns_server_conv_to_sig_num(DNS_SERVER_SIG_TASK_WATCHDOG_FEED),
        pdMS_TO_TICKS(CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000U / 3U));

    dns_server_wdt_add_and_start();

    /* Start loop to process DNS requests */
    for (;;)
    {
        os_signal_events_t sig_events = { 0 };
        if (os_signal_wait_with_timeout(g_p_dns_server_sig, OS_DELTA_TICKS_IMMEDIATE, &sig_events))
        {
            bool flag_stop = false;
            for (;;)
            {
                const os_signal_num_e sig_num = os_signal_num_get_next(&sig_events);
                if (OS_SIGNAL_NUM_NONE == sig_num)
                {
                    break;
                }
                const dns_server_sig_e dns_server_sig = dns_server_conv_from_sig_num(sig_num);
                switch (dns_server_sig)
                {
                    case DNS_SERVER_SIG_STOP:
                        LOG_INFO("Got signal STOP");
                        flag_stop = true;
                        break;
                    case DNS_SERVER_SIG_TASK_WATCHDOG_FEED:
                    {
                        const esp_err_t err = esp_task_wdt_reset();
                        if (ESP_OK != err)
                        {
                            LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
                        }
                        break;
                    }
                }
            }
            if (flag_stop)
            {
                break;
            }
        }
        dns_server_handle_req(socket_fd, &ip_resolved);

        taskYIELD(); /* allows the freeRTOS scheduler to take over if needed. DNS daemon should not be taxing on the
                        system */
    }
    LOG_INFO("Stop DNS Server");

    LOG_INFO("TaskWatchdog: Unregister current thread");
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());
    LOG_INFO("TaskWatchdog: Stop timer");
    os_timer_sig_periodic_stop(g_p_dns_server_timer_sig_watchdog_feed);
    LOG_INFO("TaskWatchdog: Delete timer");
    os_timer_sig_periodic_delete(&g_p_dns_server_timer_sig_watchdog_feed);

    LOG_INFO("Close socket");
    close(socket_fd);
    dns_server_sig_unregister_cur_thread();
}
