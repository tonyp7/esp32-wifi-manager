/**
 * @file ap_ssid.c
 * @author TheSomeMan
 * @date 2020-08-27
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "ap_ssid.h"
#include <stdio.h>
#include "wifi_manager_defs.h"

void
ap_ssid_generate(char *p_buf, const size_t buf_size, const char *p_orig_ap_ssid, const ap_mac_t *p_ap_mac)
{
    const size_t idx_mac_last_byte        = (sizeof(p_ap_mac->mac) / sizeof(p_ap_mac->mac[0])) - 1;
    const size_t idx_mac_penultimate_byte = idx_mac_last_byte - 1;
    const size_t mac_suffix_len           = 5;
    snprintf(
        p_buf,
        buf_size,
        "%.*s %02X%02X",
        MAX_SSID_SIZE - (int32_t)mac_suffix_len - 1,
        p_orig_ap_ssid,
        p_ap_mac->mac[idx_mac_penultimate_byte],
        p_ap_mac->mac[idx_mac_last_byte]);
}
