/**
 * @file wifi_sta_config.c
 * @author TheSomeMan
 * @date 2020-11-15
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifi_sta_config.h"
#include <stdbool.h>
#include <string.h>
#include "nvs.h"
#include "esp_wifi_types.h"
#include "wifi_manager_defs.h"
#include "log.h"

static const char TAG[] = "wifi_manager";

static const char wifi_manager_nvs_namespace[] = "espwifimgr";

static const wifi_settings_t g_wifi_settings_default = {
    .ap_ssid              = DEFAULT_AP_SSID,
    .ap_pwd               = DEFAULT_AP_PASSWORD,
    .ap_channel           = DEFAULT_AP_CHANNEL,
    .ap_ssid_hidden       = DEFAULT_AP_SSID_HIDDEN,
    .ap_bandwidth         = DEFAULT_AP_BANDWIDTH,
    .sta_only             = DEFAULT_STA_ONLY,
    .sta_power_save       = DEFAULT_STA_POWER_SAVE,
    .sta_static_ip        = 0,
    .sta_static_ip_config = {
        .ip               = { 0 },
        .netmask          = { 0 },
        .gw               = { 0 },
    },
};

/**
 * The actual WiFi settings in use
 */
static wifi_settings_t g_wifi_settings = {
    .ap_ssid              = DEFAULT_AP_SSID,
    .ap_pwd               = DEFAULT_AP_PASSWORD,
    .ap_channel           = DEFAULT_AP_CHANNEL,
    .ap_ssid_hidden       = DEFAULT_AP_SSID_HIDDEN,
    .ap_bandwidth         = DEFAULT_AP_BANDWIDTH,
    .sta_only             = DEFAULT_STA_ONLY,
    .sta_power_save       = DEFAULT_STA_POWER_SAVE,
    .sta_static_ip        = 0,
    .sta_static_ip_config = {
        .ip               = { 0 },
        .netmask          = { 0 },
        .gw               = { 0 },
    },
};

static wifi_config_t g_wifi_config_sta;

_Static_assert(
    MAX_SSID_SIZE == sizeof(g_wifi_config_sta.sta.ssid),
    "sizeof(g_wifi_config_sta.sta.ssid) == MAX_SSID_SIZE");
_Static_assert(
    MAX_PASSWORD_SIZE == sizeof(g_wifi_config_sta.sta.password),
    "sizeof(g_wifi_config_sta.sta.password) == MAX_PASSWORD_SIZE");

void
wifi_sta_config_init(void)
{
    memset(&g_wifi_config_sta, 0x00, sizeof(g_wifi_config_sta));
    memset(&g_wifi_settings.sta_static_ip_config, 0x00, sizeof(tcpip_adapter_ip_info_t));
}

static bool
wifi_sta_nvs_open(const nvs_open_mode_t open_mode, nvs_handle_t *p_handle)
{
    const char *    nvs_name = wifi_manager_nvs_namespace;
    const esp_err_t err      = nvs_open(nvs_name, open_mode, p_handle);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "Can't open '%s' NVS namespace", nvs_name);
        return false;
    }
    return true;
}

static bool
wifi_sta_nvs_set_blob(const nvs_handle_t handle, const char *key, const void *value, const size_t length)
{
    const esp_err_t esp_err = nvs_set_blob(handle, key, value, length);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "nvs_set_blob failed for key '%s'", key);
        return false;
    }
    return true;
}

static bool
wifi_sta_nvs_get_blob(const nvs_handle_t handle, const char *key, void *p_out_buf, size_t length)
{
    const esp_err_t esp_err = nvs_get_blob(handle, key, p_out_buf, &length);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "nvs_get_blob failed for key '%s'", key);
        return false;
    }
    return true;
}

static bool
wifi_sta_nvs_commit(const nvs_handle_t handle)
{
    const esp_err_t esp_err = nvs_commit(handle);
    if (ESP_OK != esp_err)
    {
        LOG_ERR_ESP(esp_err, "%s failed", "nvs_commit");
        return false;
    }
    return true;
}

static bool
wifi_sta_config_set_by_handle(
    const nvs_handle       handle,
    const char *           p_ssid,
    const char *           p_password,
    const wifi_settings_t *p_wifi_settings)
{
    if (!wifi_sta_nvs_set_blob(handle, "ssid", p_ssid, MAX_SSID_SIZE))
    {
        return false;
    }
    if (!wifi_sta_nvs_set_blob(handle, "password", p_password, MAX_PASSWORD_SIZE))
    {
        return false;
    }
    if (!wifi_sta_nvs_set_blob(handle, "settings", p_wifi_settings, sizeof(*p_wifi_settings)))
    {
        return false;
    }
    return true;
}

bool
wifi_sta_config_save(void)
{
    LOG_INFO("About to save config to flash");

    nvs_handle handle = 0;
    if (!wifi_sta_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "wifi_sta_nvs_open");
        return false;
    }
    const bool res = wifi_sta_config_set_by_handle(
        handle,
        (const char *)g_wifi_config_sta.sta.ssid,
        (const char *)g_wifi_config_sta.sta.password,
        &g_wifi_settings);

    (void)wifi_sta_nvs_commit(handle);

    nvs_close(handle);

    if (!res)
    {
        LOG_ERR("%s failed", "wifi_sta_config_set_by_handle");
        return false;
    }

    LOG_DBG("wifi_sta_config: ssid:%s password:%s", g_wifi_config_sta.sta.ssid, g_wifi_config_sta.sta.password);
    LOG_DBG("wifi_settings: SoftAP_ssid: %s", g_wifi_settings.ap_ssid);
    LOG_DBG("wifi_settings: SoftAP_pwd: %s", g_wifi_settings.ap_pwd);
    LOG_DBG("wifi_settings: SoftAP_channel: %i", g_wifi_settings.ap_channel);
    LOG_DBG("wifi_settings: SoftAP_hidden (1 = yes): %i", g_wifi_settings.ap_ssid_hidden);
    LOG_DBG("wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i", g_wifi_settings.ap_bandwidth);
    LOG_DBG("wifi_settings: sta_only (0 = APSTA, 1 = STA when connected): %i", g_wifi_settings.sta_only);
    LOG_DBG("wifi_settings: sta_power_save (1 = yes): %i", g_wifi_settings.sta_power_save);
    LOG_DBG("wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip): %i", g_wifi_settings.sta_static_ip);
    LOG_DBG("wifi_settings: sta_ip_addr: %s", ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.ip));
    LOG_DBG("wifi_settings: sta_gw_addr: %s", ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.gw));
    LOG_DBG("wifi_settings: sta_netmask: %s", ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.netmask));
    return ESP_OK;
}

bool
wifi_sta_config_clear(void)
{
    LOG_INFO("About to clear config in flash");

    memset(&g_wifi_config_sta, 0, sizeof(g_wifi_config_sta));
    g_wifi_config_sta.sta.ssid[0]     = '\0';
    g_wifi_config_sta.sta.password[0] = '\0';

    g_wifi_settings = g_wifi_settings_default;

    return wifi_sta_config_save();
}

static bool
wifi_sta_config_read_by_handle(
    const nvs_handle handle,
    char *           p_ssid,
    char *           p_password,
    wifi_settings_t *p_wifi_settings)
{
    if (!wifi_sta_nvs_get_blob(handle, "ssid", p_ssid, MAX_SSID_SIZE))
    {
        return false;
    }
    if (!wifi_sta_nvs_get_blob(handle, "password", p_password, MAX_PASSWORD_SIZE))
    {
        return false;
    }
    if (!wifi_sta_nvs_get_blob(handle, "settings", p_wifi_settings, sizeof(*p_wifi_settings)))
    {
        return false;
    }
    return true;
}

bool
wifi_sta_config_fetch(void)
{
    nvs_handle handle = 0;
    if (!wifi_sta_nvs_open(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "wifi_sta_nvs_open");
        return false;
    }
    memset(&g_wifi_config_sta, 0x00, sizeof(g_wifi_config_sta));
    const bool res = wifi_sta_config_read_by_handle(
        handle,
        (char *)g_wifi_config_sta.sta.ssid,
        (char *)g_wifi_config_sta.sta.password,
        &g_wifi_settings);

    nvs_close(handle);

    if (!res)
    {
        LOG_ERR("%s failed", "wifi_sta_config_read_by_handle");
        return false;
    }

    LOG_INFO("wifi_sta_config_fetch: ssid:%s password:%s", g_wifi_config_sta.sta.ssid, g_wifi_config_sta.sta.password);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_ssid:%s", g_wifi_settings.ap_ssid);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_pwd:%s", g_wifi_settings.ap_pwd);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_channel:%i", g_wifi_settings.ap_channel);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_hidden (1 = yes):%i", g_wifi_settings.ap_ssid_hidden);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz)%i", g_wifi_settings.ap_bandwidth);
    LOG_INFO("wifi_sta_config_fetch: sta_only (0 = APSTA, 1 = STA when connected):%i", g_wifi_settings.sta_only);
    LOG_INFO("wifi_sta_config_fetch: sta_power_save (1 = yes):%i", g_wifi_settings.sta_power_save);
    LOG_INFO("wifi_sta_config_fetch: sta_static_ip (0 = dhcp client, 1 = static ip):%i", g_wifi_settings.sta_static_ip);
    LOG_INFO(
        "wifi_sta_config_fetch: sta_static_ip_config: IP: %s , GW: %s , Mask: %s",
        ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.ip),
        ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.gw),
        ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.netmask));
    LOG_INFO("wifi_sta_config_fetch: sta_ip_addr: %s", ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.ip));
    LOG_INFO("wifi_sta_config_fetch: sta_gw_addr: %s", ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.gw));
    LOG_INFO("wifi_sta_config_fetch: sta_netmask: %s", ip4addr_ntoa(&g_wifi_settings.sta_static_ip_config.netmask));

    return ('\0' != g_wifi_config_sta.sta.ssid[0]) ? true : false;
}

wifi_config_t
wifi_sta_config_get_copy(void)
{
    return g_wifi_config_sta;
}

const wifi_settings_t *
wifi_sta_config_get_wifi_settings(void)
{
    return &g_wifi_settings;
}

wifi_ssid_t
wifi_sta_config_get_ssid(void)
{
    const wifi_config_t *p_wifi_config = &g_wifi_config_sta;

    wifi_ssid_t ssid = { 0 };
    snprintf(&ssid.ssid_buf[0], sizeof(ssid.ssid_buf), "%s", (const char *)&p_wifi_config->sta.ssid[0]);
    return ssid;
}

void
wifi_sta_config_set_ssid_and_password(
    const char * p_ssid,
    const size_t ssid_len,
    const char * p_password,
    const size_t password_len)
{
    wifi_config_t *p_wifi_config = &g_wifi_config_sta;

    memset(p_wifi_config, 0x00, sizeof(*p_wifi_config));
    snprintf((char *)p_wifi_config->sta.ssid, sizeof(p_wifi_config->sta.ssid), "%.*s", ssid_len, p_ssid);
    snprintf(
        (char *)p_wifi_config->sta.password,
        sizeof(p_wifi_config->sta.password),
        "%.*s",
        password_len,
        p_password);
}
