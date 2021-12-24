/**
 * @file sta_ip_unsafe.h
 * @author TheSomeMan
 * @date 2020-08-25
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_STA_IP_UNSAFE_H
#define RUUVI_ESP32_WIFI_MANAGER_STA_IP_UNSAFE_H

#include <stdint.h>
#include "sta_ip.h"

#ifdef __cplusplus
extern "C" {
#endif

void
sta_ip_unsafe_init(void);

void
sta_ip_unsafe_deinit(void);

void
sta_ip_unsafe_set(const sta_ip_address_t ip_addr);

void
sta_ip_unsafe_reset(void);

sta_ip_string_t
sta_ip_unsafe_get_copy(void);

sta_ip_address_t
sta_ip_unsafe_conv_str_to_ip(const char *const p_ip_addr_str);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_STA_IP_UNSAFE_H
