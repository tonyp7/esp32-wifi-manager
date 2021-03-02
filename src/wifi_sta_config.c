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
#include "os_mutex.h"
#define LOG_LOCAL_LEVEL LOG_LEVEL_DEBUG
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

typedef struct wifiman_sta_config_t
{
    wifi_settings_t wifi_settings;
    wifi_config_t   wifi_config_sta;
} wifiman_sta_config_t;

static wifiman_sta_config_t g_wifi_sta_config;
static os_mutex_static_t    g_wifi_sta_config_mutex_mem;
static os_mutex_t           g_wifi_sta_config_mutex;

_Static_assert(
    MAX_SSID_SIZE == sizeof(g_wifi_sta_config.wifi_config_sta.sta.ssid),
    "sizeof(g_wifi_config_sta.sta.ssid) == MAX_SSID_SIZE");
_Static_assert(
    MAX_PASSWORD_SIZE == sizeof(g_wifi_sta_config.wifi_config_sta.sta.password),
    "sizeof(g_wifi_config_sta.sta.password) == MAX_PASSWORD_SIZE");

static wifiman_sta_config_t *
wifiman_sta_config_lock(void)
{
    if (NULL == g_wifi_sta_config_mutex)
    {
        g_wifi_sta_config_mutex = os_mutex_create_static(&g_wifi_sta_config_mutex_mem);
    }
    os_mutex_lock(g_wifi_sta_config_mutex);
    return &g_wifi_sta_config;
}

void
wifiman_sta_config_unlock(wifiman_sta_config_t **pp_cfg)
{
    *pp_cfg = NULL;
    os_mutex_unlock(g_wifi_sta_config_mutex);
}

static bool
wifiman_sta_config_transaction(
    bool (*cb_func)(wifiman_sta_config_t *const p_cfg, void *const p_param),
    void *const p_param)
{
    wifiman_sta_config_t *p_cfg = wifiman_sta_config_lock();
    const bool            res   = cb_func(p_cfg, p_param);
    wifiman_sta_config_unlock(&p_cfg);
    return res;
}

static void
wifiman_sta_config_safe_transaction(
    void (*cb_func)(wifiman_sta_config_t *const p_cfg, void *const p_param),
    void *const p_param)
{
    wifiman_sta_config_t *p_cfg = wifiman_sta_config_lock();
    cb_func(p_cfg, p_param);
    wifiman_sta_config_unlock(&p_cfg);
}

static void
wifiman_sta_config_safe_transaction_without_param(void (*cb_func)(wifiman_sta_config_t *const p_cfg))
{
    wifiman_sta_config_t *p_cfg = wifiman_sta_config_lock();
    cb_func(p_cfg);
    wifiman_sta_config_unlock(&p_cfg);
}

static void
wifiman_sta_config_do_clear(wifiman_sta_config_t *const p_cfg)
{
    memset(&p_cfg->wifi_config_sta, 0x00, sizeof(p_cfg->wifi_config_sta));
    p_cfg->wifi_config_sta.sta.ssid[0]     = '\0';
    p_cfg->wifi_config_sta.sta.password[0] = '\0';

    p_cfg->wifi_settings = g_wifi_settings_default;
    memset(&p_cfg->wifi_settings.sta_static_ip_config, 0x00, sizeof(tcpip_adapter_ip_info_t));
}

void
wifi_sta_config_init(void)
{
    wifiman_sta_config_safe_transaction_without_param(&wifiman_sta_config_do_clear);
}

bool
wifi_sta_config_clear(void)
{
    LOG_INFO("About to clear config in flash");
    wifiman_sta_config_safe_transaction_without_param(&wifiman_sta_config_do_clear);
    return wifi_sta_config_save();
}

static bool
wifi_sta_nvs_open(const nvs_open_mode_t open_mode, nvs_handle_t *const p_handle)
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
wifi_sta_nvs_get_blob(const nvs_handle_t handle, const char *const key, void *const p_out_buf, size_t length)
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
    const nvs_handle             handle,
    const char *const            p_ssid,
    const char *const            p_password,
    const wifi_settings_t *const p_wifi_settings)
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

static bool
wifi_sta_config_do_save(wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    wifiman_sta_config_t *const p_cfg_copy = p_param;

    nvs_handle handle = 0;
    if (!wifi_sta_nvs_open(NVS_READWRITE, &handle))
    {
        LOG_ERR("%s failed", "wifi_sta_nvs_open");
        return false;
    }
    const bool res = wifi_sta_config_set_by_handle(
        handle,
        (const char *)p_cfg->wifi_config_sta.sta.ssid,
        (const char *)p_cfg->wifi_config_sta.sta.password,
        &p_cfg->wifi_settings);

    (void)wifi_sta_nvs_commit(handle);

    nvs_close(handle);

    if (!res)
    {
        LOG_ERR("%s failed", "wifi_sta_config_set_by_handle");
        return false;
    }

    *p_cfg_copy = *p_cfg;
    return true;
}

bool
wifi_sta_config_save(void)
{
    LOG_INFO("About to save config to flash");

    wifiman_sta_config_t cfg = { 0 };
    if (!wifiman_sta_config_transaction(&wifi_sta_config_do_save, &cfg))
    {
        LOG_ERR("%s failed", "wifi_sta_config_do_save");
        return false;
    }

    LOG_DBG("wifi_sta_config: ssid:%s password:%s", cfg.wifi_config_sta.sta.ssid, cfg.wifi_config_sta.sta.password);
    LOG_DBG("wifi_settings: SoftAP_ssid: %s", cfg.wifi_settings.ap_ssid);
    LOG_DBG("wifi_settings: SoftAP_pwd: %s", cfg.wifi_settings.ap_pwd);
    LOG_DBG("wifi_settings: SoftAP_channel: %i", cfg.wifi_settings.ap_channel);
    LOG_DBG("wifi_settings: SoftAP_hidden (1 = yes): %i", cfg.wifi_settings.ap_ssid_hidden);
    LOG_DBG("wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i", cfg.wifi_settings.ap_bandwidth);
    LOG_DBG("wifi_settings: sta_only (0 = APSTA, 1 = STA when connected): %i", cfg.wifi_settings.sta_only);
    LOG_DBG("wifi_settings: sta_power_save (1 = yes): %i", cfg.wifi_settings.sta_power_save);
    LOG_DBG("wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip): %i", cfg.wifi_settings.sta_static_ip);
    LOG_DBG("wifi_settings: sta_ip_addr: %s", ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.ip));
    LOG_DBG("wifi_settings: sta_gw_addr: %s", ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.gw));
    LOG_DBG("wifi_settings: sta_netmask: %s", ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.netmask));
    return true;
}

static bool
wifi_sta_config_read_by_handle(
    const nvs_handle       handle,
    char *const            p_ssid,
    char *const            p_password,
    wifi_settings_t *const p_wifi_settings)
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

static bool
wifi_sta_config_do_fetch(wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    wifiman_sta_config_t *const p_cfg_copy = p_param;

    nvs_handle handle = 0;
    if (!wifi_sta_nvs_open(NVS_READONLY, &handle))
    {
        LOG_ERR("%s failed", "wifi_sta_nvs_open");
        return false;
    }
    memset(&p_cfg->wifi_config_sta, 0x00, sizeof(p_cfg->wifi_config_sta));
    const bool res = wifi_sta_config_read_by_handle(
        handle,
        (char *)p_cfg->wifi_config_sta.sta.ssid,
        (char *)p_cfg->wifi_config_sta.sta.password,
        &p_cfg->wifi_settings);

    nvs_close(handle);

    if (!res)
    {
        LOG_ERR("%s failed", "wifi_sta_config_read_by_handle");
        return false;
    }
    *p_cfg_copy = *p_cfg;
    return true;
}

bool
wifi_sta_config_fetch(void)
{
    wifiman_sta_config_t cfg = { 0 };
    if (!wifiman_sta_config_transaction(&wifi_sta_config_do_fetch, &cfg))
    {
        LOG_ERR("%s failed", "wifi_sta_config_do_fetch");
        return false;
    }

    LOG_INFO(
        "wifi_sta_config_fetch: ssid:%s password:%s",
        cfg.wifi_config_sta.sta.ssid,
        cfg.wifi_config_sta.sta.password);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_ssid:%s", cfg.wifi_settings.ap_ssid);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_pwd:%s", cfg.wifi_settings.ap_pwd);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_channel:%i", cfg.wifi_settings.ap_channel);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_hidden (1 = yes):%i", cfg.wifi_settings.ap_ssid_hidden);
    LOG_INFO("wifi_sta_config_fetch: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz)%i", cfg.wifi_settings.ap_bandwidth);
    LOG_INFO("wifi_sta_config_fetch: sta_only (0 = APSTA, 1 = STA when connected):%i", cfg.wifi_settings.sta_only);
    LOG_INFO("wifi_sta_config_fetch: sta_power_save (1 = yes):%i", cfg.wifi_settings.sta_power_save);
    LOG_INFO(
        "wifi_sta_config_fetch: sta_static_ip (0 = dhcp client, 1 = static ip):%i",
        cfg.wifi_settings.sta_static_ip);
    LOG_INFO(
        "wifi_sta_config_fetch: sta_static_ip_config: IP: %s , GW: %s , Mask: %s",
        ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.ip),
        ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.gw),
        ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.netmask));
    LOG_INFO("wifi_sta_config_fetch: sta_ip_addr: %s", ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.ip));
    LOG_INFO("wifi_sta_config_fetch: sta_gw_addr: %s", ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.gw));
    LOG_INFO("wifi_sta_config_fetch: sta_netmask: %s", ip4addr_ntoa(&cfg.wifi_settings.sta_static_ip_config.netmask));

    return wifi_sta_config_is_ssid_configured();
}

static void
wifi_sta_config_do_get_copy(wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    wifi_config_t *const p_wifi_cfg = p_param;

    *p_wifi_cfg = p_cfg->wifi_config_sta;
}

wifi_config_t
wifi_sta_config_get_copy(void)
{
    wifi_config_t wifi_config = { 0 };
    wifiman_sta_config_safe_transaction(&wifi_sta_config_do_get_copy, &wifi_config);
    return wifi_config;
}

static void
wifi_sta_config_do_get_wifi_settings(wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    wifi_settings_t *const p_settings = p_param;

    *p_settings = p_cfg->wifi_settings;
}

wifi_settings_t
wifi_sta_config_get_wifi_settings(void)
{
    wifi_settings_t settings = { 0 };
    wifiman_sta_config_safe_transaction(&wifi_sta_config_do_get_wifi_settings, &settings);
    return settings;
}

static void
wifi_sta_config_do_check_is_ssid_configured(wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    bool *const p_flag_ssid_configured = p_param;

    *p_flag_ssid_configured = ('\0' != p_cfg->wifi_config_sta.sta.ssid[0]) ? true : false;
}

bool
wifi_sta_config_is_ssid_configured(void)
{
    bool flag_is_ssid_configured = false;
    wifiman_sta_config_safe_transaction(&wifi_sta_config_do_check_is_ssid_configured, &flag_is_ssid_configured);
    return flag_is_ssid_configured;
}

static void
wifi_sta_config_do_get_ssid(wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    wifi_ssid_t *const p_ssid = p_param;

    snprintf(&p_ssid->ssid_buf[0], sizeof(p_ssid->ssid_buf), "%s", (const char *)&p_cfg->wifi_config_sta.sta.ssid[0]);
}

wifi_ssid_t
wifi_sta_config_get_ssid(void)
{
    wifi_ssid_t ssid = { 0 };
    wifiman_sta_config_safe_transaction(&wifi_sta_config_do_get_ssid, &ssid);
    return ssid;
}

typedef struct wifiman_set_ssid_password_t
{
    const char *const p_ssid;
    const size_t      ssid_len;
    const char *const p_password;
    const size_t      password_len;
} wifiman_set_ssid_password_t;

static void
wifi_sta_config_do_set_ssid_and_password(wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    const wifiman_set_ssid_password_t *const p_info = p_param;

    memset(&p_cfg->wifi_config_sta, 0x00, sizeof(p_cfg->wifi_config_sta));
    snprintf(
        (char *)p_cfg->wifi_config_sta.sta.ssid,
        sizeof(p_cfg->wifi_config_sta.sta.ssid),
        "%.*s",
        p_info->ssid_len,
        p_info->p_ssid);
    snprintf(
        (char *)p_cfg->wifi_config_sta.sta.password,
        sizeof(p_cfg->wifi_config_sta.sta.password),
        "%.*s",
        p_info->password_len,
        p_info->p_password);
}

void
wifi_sta_config_set_ssid_and_password(
    const char *const p_ssid,
    const size_t      ssid_len,
    const char *const p_password,
    const size_t      password_len)
{
    wifiman_set_ssid_password_t info = {
        .p_ssid       = p_ssid,
        .ssid_len     = ssid_len,
        .p_password   = p_password,
        .password_len = password_len,
    };
    wifiman_sta_config_safe_transaction(&wifi_sta_config_do_set_ssid_and_password, &info);
}
