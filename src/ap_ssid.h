/**
 * @file ap_ssid.h
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_AP_SSID_H
#define RUUVI_ESP32_WIFI_MANAGER_AP_SSID_H

#include <stddef.h>
#include "mac_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

void
ap_ssid_generate(char *p_buf, const size_t buf_size, const char *p_orig_ap_ssid, const mac_address_bin_t *p_ap_mac);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_AP_SSID_H
