/**
 * @file sta_ip_safe.c
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "sta_ip_safe.h"
#include "os_mutex.h"
#include "sta_ip_unsafe.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
#include "log.h"

static const char TAG[] = "wifi_manager";

static os_mutex_t        g_sta_ip_safe_mutex;
static os_mutex_static_t g_sta_ip_safe_mutex_mem;

STA_IP_SAFE_STATIC
os_mutex_t
sta_ip_safe_mutex_get(void)
{
    if (NULL == g_sta_ip_safe_mutex)
    {
        g_sta_ip_safe_mutex = os_mutex_create_static(&g_sta_ip_safe_mutex_mem);
    }
    return g_sta_ip_safe_mutex;
}

STA_IP_SAFE_STATIC
void
sta_ip_safe_lock(void)
{
    os_mutex_t h_mutex = sta_ip_safe_mutex_get();
    os_mutex_lock(h_mutex);
}

STA_IP_SAFE_STATIC
void
sta_ip_safe_unlock(void)
{
    os_mutex_t h_mutex = sta_ip_safe_mutex_get();
    os_mutex_unlock(h_mutex);
}

void
sta_ip_safe_init(void)
{
    sta_ip_safe_lock();
    sta_ip_unsafe_init();
    sta_ip_safe_unlock();
    sta_ip_safe_reset();
}

void
sta_ip_safe_deinit(void)
{
    sta_ip_safe_lock();
    sta_ip_unsafe_deinit();
    sta_ip_safe_unlock();
}

void
sta_ip_safe_set(const sta_ip_address_t ip)
{
    sta_ip_safe_lock();
    sta_ip_unsafe_set(ip);
    sta_ip_safe_unlock();
    const sta_ip_string_t ip_str = sta_ip_safe_get();
    LOG_INFO("Set STA IP String to: %s", ip_str.buf);
}

void
sta_ip_safe_reset(void)
{
    sta_ip_safe_lock();
    sta_ip_unsafe_reset();
    sta_ip_safe_unlock();
    const sta_ip_string_t ip_str = sta_ip_safe_get();
    LOG_INFO("Set STA IP String to: %s", ip_str.buf);
}

sta_ip_string_t
sta_ip_safe_get(void)
{
    sta_ip_safe_lock();
    const sta_ip_string_t ip_str = sta_ip_unsafe_get_copy();
    sta_ip_safe_unlock();
    return ip_str;
}

sta_ip_address_t
sta_ip_safe_conv_str_to_ip(const char *p_ip_addr_str)
{
    return sta_ip_unsafe_conv_str_to_ip(p_ip_addr_str);
}
