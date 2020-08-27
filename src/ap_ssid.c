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
    snprintf(p_buf, buf_size, "%.*s %02X%02X", MAX_SSID_SIZE - 6, p_orig_ap_ssid, p_ap_mac->mac[4], p_ap_mac->mac[5]);
}