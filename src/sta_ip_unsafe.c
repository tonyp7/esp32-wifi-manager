/**
 * @file sta_ip_unsafe.c
 * @author TheSomeMan
 * @date 2020-08-25
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "sta_ip_unsafe.h"
#include <stdint.h>
#include "lwip/sockets.h"
#include "esp_err.h"
#include "esp_netif.h"

static char g_sta_ip_str_buf[IP4ADDR_STRLEN_MAX + 1];

_Static_assert(
    sizeof(sta_ip_string_t) >= sizeof(g_sta_ip_str_buf),
    "size of sta_ip_string_t less than IP4ADDR_STRLEN_MAX+1");

void
sta_ip_unsafe_init(void)
{
    sta_ip_unsafe_reset();
}

void
sta_ip_unsafe_deinit(void)
{
    memset(g_sta_ip_str_buf, 0, sizeof(g_sta_ip_str_buf));
}

sta_ip_string_t
sta_ip_unsafe_get_copy(void)
{
    sta_ip_string_t ipv4_str = { 0 };
    snprintf(ipv4_str.buf, sizeof(ipv4_str.buf), "%s", g_sta_ip_str_buf);
    return ipv4_str;
}

void
sta_ip_unsafe_set(const sta_ip_address_t ip_addr)
{
    const esp_ip4_addr_t addr_ip4 = {
        .addr = ip_addr,
    };
    esp_ip4addr_ntoa(&addr_ip4, g_sta_ip_str_buf, sizeof(g_sta_ip_str_buf));
}

void
sta_ip_unsafe_reset(void)
{
    sta_ip_unsafe_set(0);
}

uint32_t
sta_ip_unsafe_conv_str_to_ip(const char *const p_ip_addr_str)
{
    esp_ip4_addr_t addr_ip4 = { 0 };
    addr_ip4.addr           = esp_ip4addr_aton(p_ip_addr_str);
    return addr_ip4.addr;
}
