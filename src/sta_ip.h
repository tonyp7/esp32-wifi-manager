/**
 * @file sta_ip.h
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_STA_IP_H
#define RUUVI_ESP32_WIFI_MANAGER_STA_IP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t sta_ip_address_t;

typedef struct sta_ip_string
{
    char buf[17];
} sta_ip_string_t;

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_STA_IP_H
