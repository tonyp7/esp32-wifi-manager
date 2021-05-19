/**
 * @file sta_ip.h
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_STA_IP_H
#define RUUVI_ESP32_WIFI_MANAGER_STA_IP_H

#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STA_IP_STRING_SIZE (17U)

typedef uint32_t sta_ip_address_t;

typedef struct sta_ip_string_t
{
    char buf[STA_IP_STRING_SIZE];
} sta_ip_string_t;

static inline bool
sta_ip_cmp(const sta_ip_string_t *const p_ip1, const sta_ip_string_t *const p_ip2)
{
    if (0 != strcmp(p_ip1->buf, p_ip2->buf))
    {
        return false;
    }
    return true;
}

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_STA_IP_H
