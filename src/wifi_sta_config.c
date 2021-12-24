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

// Warning: Debug log level prints out the passwords as a "plaintext" so accidents won't happen.
#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
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

typedef bool (*wifiman_sta_config_callback_t)(wifiman_sta_config_t *const p_cfg, void *const p_param);
typedef void (*wifiman_sta_config_callback_void_t)(wifiman_sta_config_t *const p_cfg, void *const p_param);
typedef void (*wifiman_sta_const_config_callback_void_t)(const wifiman_sta_config_t *const p_cfg, void *const p_param);
typedef bool (*wifiman_sta_config_callback_cptr_t)(wifiman_sta_config_t *const p_cfg, const void *const p_param);
typedef void (*wifiman_sta_config_callback_void_cptr_t)(wifiman_sta_config_t *const p_cfg, const void *const p_param);
typedef void (*wifiman_sta_config_callback_without_param_t)(wifiman_sta_config_t *const p_cfg);

static wifiman_sta_config_t g_wifi_sta_config;
static os_mutex_static_t    g_wifi_sta_config_mutex_mem;
static os_mutex_t           g_wifi_sta_config_mutex;
static wifi_ssid_t          g_wifi_ap_ssid;

_Static_assert(
    MAX_SSID_SIZE == sizeof(g_wifi_sta_config.wifi_config_sta.sta.ssid),
    "sizeof(g_wifi_config_sta.sta.ssid) == MAX_SSID_SIZE");
_Static_assert(
    MAX_PASSWORD_SIZE == sizeof(g_wifi_sta_config.wifi_config_sta.sta.password),
    "sizeof(g_wifi_config_sta.sta.password) == MAX_PASSWORD_SIZE");

static bool
wifi_sta_config_do_save(wifiman_sta_config_t *const p_cfg, void *const p_param);

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

void
wifiman_sta_const_config_unlock(const wifiman_sta_config_t **pp_cfg)
{
    *pp_cfg = NULL;
    os_mutex_unlock(g_wifi_sta_config_mutex);
}

static bool
wifiman_sta_config_transaction(wifiman_sta_config_callback_t cb_func, void *const p_param)
{
    wifiman_sta_config_t *p_cfg = wifiman_sta_config_lock();
    const bool            res   = cb_func(p_cfg, p_param);
    wifiman_sta_config_unlock(&p_cfg);
    return res;
}

static void
wifiman_sta_config_safe_transaction(wifiman_sta_config_callback_void_t cb_func, void *const p_param)
{
    wifiman_sta_config_t *p_cfg = wifiman_sta_config_lock();
    cb_func(p_cfg, p_param);
    wifiman_sta_config_unlock(&p_cfg);
}

static void
wifiman_sta_const_config_safe_transaction(wifiman_sta_const_config_callback_void_t cb_func, void *const p_param)
{
    const wifiman_sta_config_t *p_cfg = wifiman_sta_config_lock();
    cb_func(p_cfg, p_param);
    wifiman_sta_const_config_unlock(&p_cfg);
}

static void
wifiman_sta_config_safe_transaction_with_const_param(
    wifiman_sta_config_callback_void_cptr_t cb_func,
    const void *const                       p_param)
{
    wifiman_sta_config_t *p_cfg = wifiman_sta_config_lock();
    cb_func(p_cfg, p_param);
    wifiman_sta_config_unlock(&p_cfg);
}

static void
wifiman_sta_config_safe_transaction_without_param(wifiman_sta_config_callback_without_param_t cb_func)
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
    snprintf((char *)p_cfg->wifi_settings.ap_ssid, sizeof(p_cfg->wifi_settings.ap_ssid), "%s", g_wifi_ap_ssid.ssid_buf);
    memset(&p_cfg->wifi_settings.sta_static_ip_config, 0x00, sizeof(esp_netif_ip_info_t));
}

static void
wifiman_sta_config_do_init(wifiman_sta_config_t *const p_cfg, const void *const p_param)
{
    const wifi_ssid_t *const p_gw_wifi_ssid = p_param;
    if (NULL != p_gw_wifi_ssid)
    {
        g_wifi_ap_ssid = *p_gw_wifi_ssid;
    }
    else
    {
        g_wifi_ap_ssid.ssid_buf[0] = '\0';
    }

    wifiman_sta_config_do_clear(p_cfg);
}

void
wifi_sta_config_init(const wifi_ssid_t *const p_gw_wifi_ssid)
{
    wifiman_sta_config_safe_transaction_with_const_param(&wifiman_sta_config_do_init, p_gw_wifi_ssid);
}

bool
wifi_sta_config_clear(void)
{
    LOG_INFO("About to clear config in flash");
    wifiman_sta_config_safe_transaction_without_param(&wifiman_sta_config_do_clear);
    return wifi_sta_config_save();
}

static bool
wifi_sta_nvs_open(const nvs_open_mode_t open_mode, nvs_handle_t *const p_handle, wifiman_sta_config_t *const p_cfg)
{
    const char *nvs_name = wifi_manager_nvs_namespace;
    esp_err_t   err      = nvs_open(nvs_name, open_mode, p_handle);
    if (ESP_OK != err)
    {
        if (ESP_ERR_NVS_NOT_INITIALIZED == err)
        {
            LOG_WARN("NVS namespace '%s': StorageState is INVALID, need to erase NVS", nvs_name);
            return false;
        }
        else if (ESP_ERR_NVS_NOT_FOUND == err)
        {
            LOG_WARN("NVS namespace '%s' doesn't exist and mode is NVS_READONLY, try to create it", nvs_name);
            wifiman_sta_config_do_init(p_cfg, NULL);
            wifiman_sta_config_do_clear(p_cfg);
            wifiman_sta_config_t cfg = { 0 };
            if (!wifi_sta_config_do_save(p_cfg, &cfg))
            {
                LOG_ERR("Failed to create the wifi-manager settings in NVS");
                return false;
            }

            LOG_INFO("NVS namespace '%s' created successfully", nvs_name);
            err = nvs_open(nvs_name, open_mode, p_handle);
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "Can't open NVS namespace: '%s'", nvs_name);
                return false;
            }
        }
        else
        {
            LOG_ERR_ESP(err, "Can't open NVS namespace: '%s'", nvs_name);
            return false;
        }
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
    if (!wifi_sta_nvs_open(NVS_READWRITE, &handle, p_cfg))
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

static const char *
wifi_sta_config_get_sta_password_for_logging(const wifiman_sta_config_t *const p_cfg)
{
    return ((LOG_LOCAL_LEVEL >= LOG_LEVEL_DEBUG) ? (const char *)p_cfg->wifi_config_sta.sta.password : "********");
}

static void
wifi_sta_config_log(const wifiman_sta_config_t *const p_cfg, const char *const p_prefix)
{
    LOG_INFO(
        "%s: ssid:%s password:%s",
        p_prefix,
        p_cfg->wifi_config_sta.sta.ssid,
        wifi_sta_config_get_sta_password_for_logging(p_cfg));
    LOG_INFO("%s: SoftAP_ssid: %s", p_prefix, p_cfg->wifi_settings.ap_ssid);
    LOG_INFO("%s: SoftAP_pwd: %s", p_prefix, p_cfg->wifi_settings.ap_pwd);
    LOG_INFO("%s: SoftAP_channel: %i", p_prefix, p_cfg->wifi_settings.ap_channel);
    LOG_INFO("%s: SoftAP_hidden (1 = yes): %i", p_prefix, p_cfg->wifi_settings.ap_ssid_hidden);
    LOG_INFO("%s: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i", p_prefix, p_cfg->wifi_settings.ap_bandwidth);
    LOG_INFO("%s: sta_only (0 = APSTA, 1 = STA when connected): %i", p_prefix, p_cfg->wifi_settings.sta_only);
    LOG_INFO("%s: sta_power_save (1 = yes): %i", p_prefix, p_cfg->wifi_settings.sta_power_save);
    LOG_INFO("%s: sta_static_ip (0 = dhcp client, 1 = static ip): %i", p_prefix, p_cfg->wifi_settings.sta_static_ip);
    wifi_ip4_addr_str_t ip_str;
    wifi_ip4_addr_str_t gw_str;
    wifi_ip4_addr_str_t netmask_str;
    esp_ip4addr_ntoa(&p_cfg->wifi_settings.sta_static_ip_config.ip, ip_str.buf, sizeof(ip_str.buf));
    esp_ip4addr_ntoa(&p_cfg->wifi_settings.sta_static_ip_config.gw, gw_str.buf, sizeof(gw_str.buf));
    esp_ip4addr_ntoa(&p_cfg->wifi_settings.sta_static_ip_config.netmask, netmask_str.buf, sizeof(netmask_str.buf));
    LOG_INFO("%s: sta_static_ip_config: IP: %s , GW: %s , Mask: %s", p_prefix, ip_str.buf, gw_str.buf, netmask_str.buf);
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
    wifi_sta_config_log(&cfg, "wifi_settings");
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
    if (!wifi_sta_nvs_open(NVS_READONLY, &handle, p_cfg))
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
    if (NULL != p_cfg_copy)
    {
        *p_cfg_copy = *p_cfg;
    }
    return true;
}

bool
wifi_sta_config_check(void)
{
    if (!wifiman_sta_config_transaction(&wifi_sta_config_do_fetch, NULL))
    {
        LOG_ERR("%s failed", "wifi_sta_config_do_fetch");
        return false;
    }
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
    wifi_sta_config_log(&cfg, "wifi_sta_config_fetch");
    return wifi_sta_config_is_ssid_configured();
}

static void
wifi_sta_config_do_get_copy(const wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    wifi_config_t *const p_wifi_cfg = p_param;

    *p_wifi_cfg = p_cfg->wifi_config_sta;
}

wifi_config_t
wifi_sta_config_get_copy(void)
{
    wifi_config_t wifi_config = { 0 };
    wifiman_sta_const_config_safe_transaction(&wifi_sta_config_do_get_copy, &wifi_config);
    return wifi_config;
}

static void
wifi_sta_config_do_get_wifi_settings(const wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    wifi_settings_t *const p_settings = p_param;

    *p_settings = p_cfg->wifi_settings;
}

wifi_settings_t
wifi_sta_config_get_wifi_settings(void)
{
    wifi_settings_t settings = { 0 };
    wifiman_sta_const_config_safe_transaction(&wifi_sta_config_do_get_wifi_settings, &settings);
    return settings;
}

static void
wifi_sta_config_do_check_is_ssid_configured(const wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    bool *const p_flag_ssid_configured = p_param;

    *p_flag_ssid_configured = ('\0' != p_cfg->wifi_config_sta.sta.ssid[0]) ? true : false;
}

bool
wifi_sta_config_is_ssid_configured(void)
{
    bool flag_is_ssid_configured = false;
    wifiman_sta_const_config_safe_transaction(&wifi_sta_config_do_check_is_ssid_configured, &flag_is_ssid_configured);
    return flag_is_ssid_configured;
}

static void
wifi_sta_config_do_get_ssid(const wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    wifi_ssid_t *const p_ssid = p_param;

    snprintf(&p_ssid->ssid_buf[0], sizeof(p_ssid->ssid_buf), "%s", (const char *)&p_cfg->wifi_config_sta.sta.ssid[0]);
}

wifi_ssid_t
wifi_sta_config_get_ssid(void)
{
    wifi_ssid_t ssid = { 0 };
    wifiman_sta_const_config_safe_transaction(&wifi_sta_config_do_get_ssid, &ssid);
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
wifi_sta_config_do_set_ssid_and_password(wifiman_sta_config_t *const p_cfg, const void *const p_param)
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
    wifiman_sta_config_safe_transaction_with_const_param(&wifi_sta_config_do_set_ssid_and_password, &info);
}

static void
wifi_sta_config_do_get_ap_ssid(const wifiman_sta_config_t *const p_cfg, void *const p_param)
{
    (void)p_cfg;
    wifi_ssid_t *const p_ap_ssid = p_param;

    snprintf(&p_ap_ssid->ssid_buf[0], sizeof(p_ap_ssid->ssid_buf), "%s", (const char *)&g_wifi_ap_ssid.ssid_buf[0]);
}

wifi_ssid_t
wifi_sta_config_get_ap_ssid(void)
{
    wifi_ssid_t ap_ssid = { 0 };
    wifiman_sta_const_config_safe_transaction(&wifi_sta_config_do_get_ap_ssid, &ap_ssid);
    return ap_ssid;
}
