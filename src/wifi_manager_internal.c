/**
 * @file wifi_manager_internal.c
 * @author TheSomeMan
 * @date 2021-11-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifi_manager_internal.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_task_wdt.h"
#include "lwip/sockets.h"
#include "os_mutex_recursive.h"
#include "os_sema.h"
#include "wifi_manager.h"
#include "wifiman_msg.h"
#include "http_server_resp.h"
#include "json_network_info.h"
#include "sta_ip_safe.h"
#include "dns_server.h"
#include "json_access_points.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "wifi_manager";

static wifi_manager_callbacks_t g_wifi_callbacks;

static os_timer_one_shot_without_arg_t *g_p_wifi_scan_timer;
static os_timer_one_shot_static_t       g_wifi_scan_timer_mem;

static os_sema_t        g_p_scan_sync_sema;
static os_sema_static_t g_scan_sync_sema_mem;

static os_mutex_recursive_t        g_p_wifi_mutex;
static os_mutex_recursive_static_t g_wifi_manager_mutex_mem;

void
wifi_manager_init_mutex(void)
{
    if (NULL == g_p_wifi_mutex)
    {
        // Init this mutex only on the first start,
        // do not free it when wifi_manager is stopped.
        g_p_wifi_mutex = os_mutex_recursive_create_static(&g_wifi_manager_mutex_mem);
    }
}

bool
wifi_manager_lock_with_timeout(const os_delta_ticks_t ticks_to_wait)
{
    assert(NULL != g_p_wifi_mutex);
    return os_mutex_recursive_lock_with_timeout(g_p_wifi_mutex, ticks_to_wait);
}

void
wifi_manager_lock(void)
{
    assert(NULL != g_p_wifi_mutex);
    os_mutex_recursive_lock(g_p_wifi_mutex);
}

void
wifi_manager_unlock(void)
{
    assert(NULL != g_p_wifi_mutex);
    os_mutex_recursive_unlock(g_p_wifi_mutex);
}

static void
wifi_manager_set_callbacks(const wifi_manager_callbacks_t *const p_callbacks)
{
    g_wifi_callbacks = *p_callbacks;
}

void
wifi_manager_cb_on_user_req(const http_server_user_req_code_e req_code)
{
    if (NULL == g_wifi_callbacks.cb_on_http_user_req)
    {
        return;
    }
    g_wifi_callbacks.cb_on_http_user_req(req_code);
}

http_server_resp_t
wifi_manager_cb_on_http_get(
    const char *const               p_path,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth)
{
    if (NULL == g_wifi_callbacks.cb_on_http_get)
    {
        return http_server_resp_404();
    }
    return g_wifi_callbacks.cb_on_http_get(p_path, flag_access_from_lan, p_resp_auth);
}

http_server_resp_t
wifi_manager_cb_on_http_post(const char *const p_path, const http_req_body_t http_body, const bool flag_access_from_lan)
{
    if (NULL == g_wifi_callbacks.cb_on_http_post)
    {
        return http_server_resp_404();
    }
    return g_wifi_callbacks.cb_on_http_post(p_path, http_body.ptr, flag_access_from_lan);
}

http_server_resp_t
wifi_manager_cb_on_http_delete(
    const char *const               p_path,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth)
{
    if (NULL == g_wifi_callbacks.cb_on_http_delete)
    {
        return http_server_resp_404();
    }
    return g_wifi_callbacks.cb_on_http_delete(p_path, flag_access_from_lan, p_resp_auth);
}

static wifi_config_t
wifi_manager_generate_ap_config(const struct wifi_settings_t *const p_wifi_settings)
{
    wifi_config_t ap_config = {
        .ap = {
            .ssid            = {
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            },
            .password        = {
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            },
            .ssid_len        = 0,
            .channel         = p_wifi_settings->ap_channel,
            .authmode        = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden     = p_wifi_settings->ap_ssid_hidden,
            .max_connection  = DEFAULT_AP_MAX_CONNECTIONS,
            .beacon_interval = DEFAULT_AP_BEACON_INTERVAL,
            .pairwise_cipher = WIFI_CIPHER_TYPE_TKIP_CCMP,
            .ftm_responder   = false,
        },
    };

    snprintf((char *)&ap_config.ap.ssid[0], sizeof(ap_config.ap.ssid), "%s", p_wifi_settings->ap_ssid);
    snprintf((char *)&ap_config.ap.password[0], sizeof(ap_config.ap.password), "%s", p_wifi_settings->ap_pwd);
    return ap_config;
}

static void
wifi_manager_esp_wifi_configure(const struct wifi_settings_t *const p_wifi_settings)
{
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_set_mode");
        return;
    }
    xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_AP_ACTIVE);
    wifi_config_t ap_config = wifi_manager_generate_ap_config(p_wifi_settings);
    err                     = esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_set_config");
        return;
    }
    err = esp_wifi_set_bandwidth(WIFI_IF_AP, p_wifi_settings->ap_bandwidth);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_set_bandwidth");
        return;
    }
    err = esp_wifi_set_ps(p_wifi_settings->sta_power_save);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_set_ps");
        return;
    }
}

static void
wifi_manager_netif_set_default_ip(void)
{
    LOG_INFO("Set default IP for WiFi AP: %s", DEFAULT_AP_IP);
    esp_netif_t *const  p_netif_ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_ip_info_t info       = { 0 };
    info.ip.addr                   = esp_ip4addr_aton(DEFAULT_AP_IP); /* access point is on a static IP */
    info.gw.addr                   = esp_ip4addr_aton(DEFAULT_AP_GATEWAY);
    info.netmask.addr              = esp_ip4addr_aton(DEFAULT_AP_NETMASK);
    esp_err_t err                  = esp_netif_dhcps_stop(p_netif_ap);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_netif_dhcps_stop");
        return;
    }
    err = esp_netif_set_ip_info(p_netif_ap, &info);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_netif_set_ip_info");
        return;
    }
    err = esp_netif_dhcps_start(p_netif_ap);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_netif_dhcps_start");
        return;
    }
}

static void
wifi_manager_netif_configure(const struct wifi_settings_t *const p_wifi_settings)
{
    esp_netif_t *const p_netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (p_wifi_settings->sta_static_ip)
    {
        wifi_ip4_addr_str_t buf_ip;
        wifi_ip4_addr_str_t buf_gw;
        wifi_ip4_addr_str_t buf_netmask;
        LOG_INFO(
            "Assigning static ip to STA interface. IP: %s , GW: %s , Mask: %s",
            esp_ip4addr_ntoa(&p_wifi_settings->sta_static_ip_config.ip, buf_ip.buf, sizeof(buf_ip.buf)),
            esp_ip4addr_ntoa(&p_wifi_settings->sta_static_ip_config.gw, buf_gw.buf, sizeof(buf_gw.buf)),
            esp_ip4addr_ntoa(&p_wifi_settings->sta_static_ip_config.netmask, buf_netmask.buf, sizeof(buf_netmask.buf)));

        /* stop DHCP client*/
        esp_err_t err = esp_netif_dhcpc_stop(p_netif_sta);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_netif_dhcpc_stop");
            return;
        }
        /* assign a static IP to the STA network interface */
        err = esp_netif_set_ip_info(p_netif_sta, &p_wifi_settings->sta_static_ip_config);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_netif_set_ip_info");
            return;
        }
    }
    else
    {
        /* start DHCP client if not started*/
        LOG_INFO("wifi_manager: Start DHCP client for STA interface. If not already running");
        esp_netif_dhcp_status_t status = 0;

        esp_err_t err = esp_netif_dhcpc_get_status(p_netif_sta, &status);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_netif_dhcpc_get_status");
            return;
        }
        if (status != ESP_NETIF_DHCP_STARTED)
        {
            err = esp_netif_dhcpc_start(p_netif_sta);
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "%s failed", "esp_netif_dhcpc_start");
                return;
            }
        }
    }
}

static void
wifi_scan_next_timer_handler(os_timer_one_shot_without_arg_t *const p_timer)
{
    (void)p_timer;
    wifiman_msg_send_ev_scan_next();
}

void
wifi_manager_event_handler(
    ATTR_UNUSED void *     p_ctx,
    const esp_event_base_t p_event_base,
    const int32_t          event_id,
    void *                 p_event_data)
{
    if (WIFI_EVENT == p_event_base)
    {
        switch (event_id)
        {
            case WIFI_EVENT_WIFI_READY:
                LOG_INFO("WIFI_EVENT_WIFI_READY");
                break;
            case WIFI_EVENT_SCAN_DONE:
                wifiman_msg_send_ev_scan_done();
                break;
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                LOG_INFO("WIFI_EVENT_STA_AUTHMODE_CHANGE");
                break;
            case WIFI_EVENT_AP_START:
                LOG_INFO("WIFI_EVENT_AP_START");
                xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
                break;
            case WIFI_EVENT_AP_STOP:
                LOG_INFO("WIFI_EVENT_AP_STOP");
                break;
            case WIFI_EVENT_AP_PROBEREQRECVED:
                break;
            case WIFI_EVENT_AP_STACONNECTED: /* a user disconnected from the SoftAP */
                LOG_INFO("WIFI_EVENT_AP_STACONNECTED");
                wifiman_msg_send_ev_ap_sta_connected();
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                LOG_INFO("WIFI_EVENT_AP_STADISCONNECTED");
                wifiman_msg_send_ev_ap_sta_disconnected();
                break;
            case WIFI_EVENT_STA_START:
                LOG_INFO("WIFI_EVENT_STA_START");
                break;
            case WIFI_EVENT_STA_STOP:
                LOG_INFO("WIFI_EVENT_STA_STOP");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                LOG_INFO("WIFI_EVENT_STA_CONNECTED");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                LOG_INFO("WIFI_EVENT_STA_DISCONNECTED");
                wifiman_msg_send_ev_disconnected(((const wifi_event_sta_disconnected_t *)p_event_data)->reason);
                break;
            default:
                break;
        }
    }
    else if (IP_EVENT == p_event_base)
    {
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                LOG_INFO("IP_EVENT_STA_GOT_IP");
                wifiman_msg_send_ev_got_ip(((const ip_event_got_ip_t *)p_event_data)->ip_info.ip.addr);
                break;
            case IP_EVENT_AP_STAIPASSIGNED:
                LOG_INFO("IP_EVENT_AP_STAIPASSIGNED");
                wifiman_msg_send_ev_ap_sta_ip_assigned();
                break;
            default:
                break;
        }
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }
}

static void
wifi_manager_set_ant_config(const wifi_manager_antenna_config_t *p_wifi_ant_config)
{
    if (NULL == p_wifi_ant_config)
    {
        return;
    }
    esp_err_t err = esp_wifi_set_ant_gpio(&p_wifi_ant_config->wifi_ant_gpio_config);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_wifi_set_ant_gpio failed");
    }
    err = esp_wifi_set_ant(&p_wifi_ant_config->wifi_ant_config);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_wifi_set_ant failed");
    }
}

static bool
wifi_manager_init_start_wifi(
    const wifi_manager_antenna_config_t *const p_wifi_ant_config,
    const wifi_ssid_t *const                   p_gw_wifi_ssid)
{
    if (!wifiman_msg_init())
    {
        LOG_ERR("%s failed", "wifiman_msg_init");
        return false;
    }

    json_access_points_init();

    for (int32_t msg_code = WIFI_MAN_MSG_CODE_NONE; msg_code < MESSAGE_CODE_COUNT; ++msg_code)
    {
        wifi_manager_set_callback((message_code_e)msg_code, NULL);
    }

    esp_err_t err = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_event_handler_register");
        return false;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler, NULL);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_event_handler_register");
        return false;
    }

    err = esp_event_handler_register(IP_EVENT, IP_EVENT_AP_STAIPASSIGNED, &wifi_manager_event_handler, NULL);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_event_handler_register");
        return false;
    }

    /* default wifi config */
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();

    err = esp_wifi_init(&wifi_init_config);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_wifi_init");
        return false;
    }

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_wifi_set_storage");
        return false;
    }

    wifi_manager_set_ant_config(p_wifi_ant_config);
    /* SoftAP - Wi-Fi Access Point configuration setup */
    wifi_manager_netif_set_default_ip();
    const wifi_settings_t wifi_settings = wifi_sta_config_get_wifi_settings();
    wifi_manager_esp_wifi_configure(&wifi_settings);

    /* STA - Wifi Station configuration setup */
    wifi_manager_netif_configure(&wifi_settings);

    /* by default the mode is STA because wifi_manager will not start the access point unless it has to! */
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_wifi_set_mode");
        return false;
    }
    xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_AP_ACTIVE);

    err = esp_wifi_start();
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_wifi_start");
        return false;
    }

    esp_netif_t *const p_netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

    LOG_INFO("Set hostname for WiFi interface: %s", p_gw_wifi_ssid->ssid_buf);
    err = esp_netif_set_hostname(p_netif_sta, p_gw_wifi_ssid->ssid_buf);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_netif_set_hostname");
        return false;
    }

    /* start wifi manager task */
    const char *   task_name   = "wifi_manager";
    const uint32_t stack_depth = 4096U;
    if (!os_task_create_finite_without_param(&wifi_manager_task, task_name, stack_depth, WIFI_MANAGER_TASK_PRIORITY))
    {
        LOG_ERR("Can't create thread: %s", task_name);
        return false;
    }
    return true;
}

bool
wifi_manager_init(
    const bool                                 flag_start_wifi,
    const bool                                 flag_start_ap_only,
    const wifi_ssid_t *const                   p_gw_wifi_ssid,
    const wifi_manager_antenna_config_t *const p_wifi_ant_config,
    const wifi_manager_callbacks_t *const      p_callbacks)
{
    LOG_INFO("WiFi manager init");
    if (wifi_manager_is_working())
    {
        LOG_ERR("wifi_manager is already running");
        return false;
    }
    xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_IS_WORKING);

    g_p_wifi_scan_timer = os_timer_one_shot_without_arg_create_static(
        &g_wifi_scan_timer_mem,
        "wifi_scan",
        pdMS_TO_TICKS(WIFI_MANAGER_DELAY_BETWEEN_SCANNING_WIFI_CHANNELS_MS),
        &wifi_scan_next_timer_handler);

    wifi_manager_set_callbacks(p_callbacks);

    wifi_sta_config_init(p_gw_wifi_ssid);
    json_network_info_init();
    sta_ip_safe_init();

    esp_err_t err = esp_event_loop_create_default();
    if (ESP_OK != err)
    {
        LOG_ERR("%s failed", "esp_event_loop_create_default");
        return false;
    }

    /* initialize the tcp stack */
    esp_netif_init();
    if (NULL == esp_netif_create_default_wifi_ap())
    {
        LOG_ERR("%s failed", "esp_netif_create_default_wifi_ap");
        return false;
    }
    if (NULL == esp_netif_create_default_wifi_sta())
    {
        LOG_ERR("%s failed", "esp_netif_create_default_wifi_sta");
        return false;
    }

    dns_server_init();

    http_server_init();
    http_server_start();

    LOG_INFO("WiFi manager init: start WiFi");
    wifi_manager_init_start_wifi(p_wifi_ant_config, p_gw_wifi_ssid);

    if (flag_start_wifi)
    {
        const bool is_ssid_configured = wifi_sta_config_fetch();
        if (is_ssid_configured && (!flag_start_ap_only))
        {
            LOG_INFO("Saved wifi found on startup. Will attempt to connect.");
            wifiman_msg_send_cmd_connect_sta(CONNECTION_REQUEST_RESTORE_CONNECTION);
        }
        else
        {
            /* no Wi-Fi saved: start soft AP! This is what should happen during a first run */
            if (flag_start_ap_only)
            {
                LOG_INFO("Force start WiFi hotspot on startup.");
            }
            else
            {
                LOG_INFO("No saved wifi found on startup. Starting access point.");
            }
            wifiman_msg_send_cmd_start_ap();
        }
    }
    return true;
}

void
wifi_manager_scan_timer_start(void)
{
    os_timer_one_shot_without_arg_start(g_p_wifi_scan_timer);
}

static const char *
wifi_manager_generate_json_access_points(void)
{
    if (wifi_manager_lock_with_timeout(pdMS_TO_TICKS(100)))
    {
        const char *const p_buf = wifi_manager_generate_access_points_json();
        wifi_manager_unlock();
        return p_buf;
    }
    return NULL;
}

const char *
wifi_manager_scan_sync(void)
{
    wifi_manager_lock();
    if (NULL != g_p_scan_sync_sema)
    {
        LOG_ERR("Another thread tries to perform the same operation");
        wifi_manager_unlock();
        return NULL;
    }
    g_p_scan_sync_sema = os_sema_create_static(&g_scan_sync_sema_mem);
    LOG_INFO("wifi_manager_scan_sync: wifiman_msg_send_cmd_start_wifi_scan");
    if (!wifiman_msg_send_cmd_start_wifi_scan())
    {
        wifi_manager_unlock();
        return NULL;
    }
    wifi_manager_unlock();

    while (!os_sema_wait_with_timeout(g_p_scan_sync_sema, pdMS_TO_TICKS(CONFIG_ESP_TASK_WDT_TIMEOUT_S * 1000U / 3U)))
    {
        const esp_err_t err = esp_task_wdt_reset();
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
        }
    }

    wifi_manager_lock();
    os_sema_delete(&g_p_scan_sync_sema);
    const char *const p_buf = wifi_manager_generate_json_access_points();
    LOG_DBG("wifi_manager_scan_sync: p_buf: %s", p_buf ? p_buf : "NULL");
    wifi_manager_unlock();

    return p_buf;
}

void
wifi_callback_on_connect_eth_cmd(void)
{
    if (NULL != g_wifi_callbacks.cb_on_connect_eth_cmd)
    {
        g_wifi_callbacks.cb_on_connect_eth_cmd();
    }
}

void
wifi_callback_on_ap_sta_connected(void)
{
    if (NULL != g_wifi_callbacks.cb_on_ap_sta_connected)
    {
        g_wifi_callbacks.cb_on_ap_sta_connected();
    }
}

void
wifi_callback_on_ap_sta_disconnected(void)
{
    if (NULL != g_wifi_callbacks.cb_on_ap_sta_disconnected)
    {
        g_wifi_callbacks.cb_on_ap_sta_disconnected();
    }
}

void
wifi_callback_on_disconnect_eth_cmd(void)
{
    if (NULL != g_wifi_callbacks.cb_on_disconnect_eth_cmd)
    {
        g_wifi_callbacks.cb_on_disconnect_eth_cmd();
    }
}

void
wifi_callback_on_disconnect_sta_cmd(void)
{
    if (NULL != g_wifi_callbacks.cb_on_disconnect_sta_cmd)
    {
        g_wifi_callbacks.cb_on_disconnect_sta_cmd();
    }
}

void
wifi_manger_notify_scan_done(void)
{
    xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
    if (NULL != g_p_scan_sync_sema)
    {
        LOG_INFO("NOTIFY: wifi scan done");
        os_sema_signal(g_p_scan_sync_sema);
    }
}
