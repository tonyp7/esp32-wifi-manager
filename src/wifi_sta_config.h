/**
 * @file wifi_sta_config.h
 * @author TheSomeMan
 * @date 2020-11-15
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFI_STA_CONFIG_H
#define WIFI_STA_CONFIG_H

#include <stdbool.h>
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "tcpip_adapter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * The actual WiFi settings in use
 */
typedef struct wifi_settings_t
{
    uint8_t                 ap_ssid[MAX_SSID_SIZE];
    uint8_t                 ap_pwd[MAX_PASSWORD_SIZE];
    uint8_t                 ap_channel;
    uint8_t                 ap_ssid_hidden;
    wifi_bandwidth_t        ap_bandwidth;
    bool                    sta_only;
    wifi_ps_type_t          sta_power_save;
    bool                    sta_static_ip;
    tcpip_adapter_ip_info_t sta_static_ip_config;
} wifi_settings_t;

/**
 * @brief Clears the current STA wifi config in RAM storage.
 */
void
wifi_sta_config_init(void);

/**
 * @brief Clears the current STA wifi config in NVS and RAM storage.
 */
bool
wifi_sta_config_clear(void);

/**
 * @brief Saves the current STA wifi config to NVS from RAM storage.
 */
bool
wifi_sta_config_save(void);

/**
 * @brief Fetch a previously STA wifi config from NVS to the RAM storage.
 * @return true if a previously saved config was found, false otherwise.
 */
bool
wifi_sta_config_fetch(void);

/**
 * @brief Get the copy of the current STA wifi config in RAM storage.
 */
wifi_config_t
wifi_sta_config_get_copy(void);

const wifi_settings_t *
wifi_sta_config_get_wifi_settings(void);

/**
 * @brief Copy SSID from the current STA wifi config in RAM.
 */
wifi_ssid_t
wifi_sta_config_get_ssid(void);

void
wifi_sta_config_set_ssid_and_password(
    const char * p_ssid,
    const size_t ssid_len,
    const char * p_password,
    const size_t password_len);

#ifdef __cplusplus
}
#endif

#endif // WIFI_STA_CONFIG_H
