/**
 * @file ap_mac.h
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_AP_MAC_H
#define RUUVI_ESP32_WIFI_MANAGER_AP_MAC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ap_mac_t
{
    uint8_t mac[6];
} ap_mac_t;

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_AP_MAC_H
