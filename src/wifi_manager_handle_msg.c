/**
 * @file wifi_manager_handle_msg.c
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
#include "lwip/dhcp.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "os_sema.h"
#include "wifi_manager.h"
#include "wifiman_msg.h"
#include "http_server_resp.h"
#include "access_points_list.h"
#include "dns_server.h"
#include "json_access_points.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

static const char TAG[] = "wifi_manager";

static wifi_manager_cb_ptr      g_wifi_cb_ptr_arr[MESSAGE_CODE_COUNT];
static wifi_manager_scan_info_t g_wifi_scan_info;
static uint16_t                 g_wifi_ap_num = MAX_AP_NUM;
static wifi_ap_record_t         g_wifi_ap_records[2 * MAX_AP_NUM];

static bool
wifi_scan_next(wifi_manager_scan_info_t *const p_scan_info)
{
    p_scan_info->cur_chan += 1;
    if (p_scan_info->cur_chan > p_scan_info->last_chan)
    {
        return true; // scanning finished
    }
    LOG_INFO(
        "Delay %u ms before scanning Wi-Fi APs on channel %u",
        (printf_uint_t)WIFI_MANAGER_DELAY_BETWEEN_SCANNING_WIFI_CHANNELS_MS,
        (printf_uint_t)p_scan_info->cur_chan);
    wifi_manager_scan_timer_start();
    return false; // scanning not finished
}

static void
wifi_handle_cmd_start_wifi_scan(void)
{
    LOG_INFO("MESSAGE: ORDER_START_WIFI_SCAN");

    /* if a scan is already in progress this message is simply ignored thanks to the
     * WIFI_MANAGER_SCAN_BIT uxBit */
    const EventBits_t uxBits = xEventGroupGetBits(g_p_wifi_manager_event_group);
    if (0 != (uxBits & WIFI_MANAGER_SCAN_BIT))
    {
        return;
    }

    xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);

    wifi_country_t  wifi_country = { 0 };
    const esp_err_t err          = esp_wifi_get_country(&wifi_country);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_get_country");
        wifi_country.schan = WIFI_MANAGER_WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL;
        wifi_country.nchan = WIFI_MANAGER_WIFI_COUNTRY_DEFAULT_NUM_CHANNELS;
    }

    wifi_manager_scan_info_t *const p_scan_info = &g_wifi_scan_info;
    p_scan_info->first_chan                     = wifi_country.schan;
    p_scan_info->last_chan                      = (wifi_country.schan + wifi_country.nchan) - 1;
    p_scan_info->cur_chan                       = 0;
    p_scan_info->num_access_points              = 0;

    if (wifi_scan_next(p_scan_info))
    {
        wifiman_msg_send_ev_scan_done();
    }
}

static void
wifi_handle_cmd_connect_eth(void)
{
    LOG_INFO("MESSAGE: ORDER_CONNECT_ETH");
    wifi_callback_on_connect_eth_cmd();
}

static void
wifi_handle_cmd_connect_sta(const wifiman_msg_param_t *const p_param)
{
    LOG_INFO("MESSAGE: ORDER_CONNECT_STA");

    /* very important: precise that this connection attempt is specifically requested.
     * Param in that case is a boolean indicating if the request was made automatically
     * by the wifi_manager.
     * */
    const connection_request_made_by_code_e conn_req = wifiman_conv_param_to_conn_req(p_param);
    if (CONNECTION_REQUEST_USER == conn_req)
    {
        xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
    }
    else if (CONNECTION_REQUEST_RESTORE_CONNECTION == conn_req)
    {
        xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
    }
    else
    {
        // MISRA C:2012, 15.7 - All if...else if constructs shall be terminated with an else statement
    }

    const EventBits_t uxBits = xEventGroupGetBits(g_p_wifi_manager_event_group);
    if (0 != (uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT))
    {
        wifiman_msg_send_cmd_disconnect_sta();
        wifiman_msg_send_cmd_connect_sta(CONNECTION_REQUEST_AUTO_RECONNECT);
    }
    else
    {
        /* update config to latest and attempt connection */
        wifi_config_t wifi_config = wifi_sta_config_get_copy();
        esp_err_t     err         = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        if (ESP_OK != err)
        {
            LOG_ERR_ESP(err, "%s failed", "esp_wifi_set_config");
        }
        else
        {
            err = esp_wifi_connect();
            if (ESP_OK != err)
            {
                LOG_ERR_ESP(err, "%s failed", "esp_wifi_connect");
            }
        }
    }
}

/**
 * @brief Handle event EVENT_STA_DISCONNECTED
 * @note this even can be posted in numerous different conditions
 *
 * 1. SSID password is wrong
 * 2. Manual disconnection ordered
 * 3. Connection lost
 *
 * Having clear understand as to WHY the event was posted is key to having an efficient wifi manager
 *
 * With wifi_manager, we determine:
 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, We consider it's a client that requested the
 *connection. When WIFI_EVENT_STA_DISCONNECTED is posted, it's probably a password/something went
 *wrong with the handshake.
 *
 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, it's a disconnection that was ASKED by the
 *client (clicking disconnect in the app) When WIFI_EVENT_STA_DISCONNECTED is posted, saved wifi is
 *erased from the NVS memory.
 *
 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT and WIFI_MANAGER_REQUEST_STA_CONNECT_BIT are NOT set,
 *it's a lost connection
 *
 *  In this version of the software, reason codes are not used. They are indicated here for
 *potential future usage.
 *
 *  REASON CODE:
 *  1       UNSPECIFIED
 *  2       AUTH_EXPIRE auth no longer valid, this smells like someone changed a password on the AP
 *  3       AUTH_LEAVE
 *  4       ASSOC_EXPIRE
 *  5       ASSOC_TOOMANY too many devices already connected to the AP => AP fails to respond
 *  6       NOT_AUTHED
 *  7       NOT_ASSOCED
 *  8       ASSOC_LEAVE
 *  9       ASSOC_NOT_AUTHED
 *  10      DISASSOC_PWRCAP_BAD
 *  11      DISASSOC_SUPCHAN_BAD
 *  12      <n/a>
 *  13      IE_INVALID
 *  14      MIC_FAILURE
 *  15      4WAY_HANDSHAKE_TIMEOUT wrong password! This was personnaly tested on my home wifi
 *          with a wrong password.
 *  16      GROUP_KEY_UPDATE_TIMEOUT
 *  17      IE_IN_4WAY_DIFFERS
 *  18      GROUP_CIPHER_INVALID
 *  19      PAIRWISE_CIPHER_INVALID
 *  20      AKMP_INVALID
 *  21      UNSUPP_RSN_IE_VERSION
 *  22      INVALID_RSN_IE_CAP
 *  23      802_1X_AUTH_FAILED  wrong password?
 *  24      CIPHER_SUITE_REJECTED
 *  200     BEACON_TIMEOUT
 *  201     NO_AP_FOUND
 *  202     AUTH_FAIL
 *  203     ASSOC_FAIL
 *  204     HANDSHAKE_TIMEOUT
 *
 *
 * @param p_param - pointer to wifiman_msg_param_t
 */
static bool
wifi_handle_ev_sta_disconnected(const wifiman_msg_param_t *const p_param)
{
    const wifiman_disconnection_reason_t reason = wifiman_conv_param_to_reason(p_param);
    LOG_INFO("MESSAGE: EVENT_STA_DISCONNECTED with Reason code: %d", reason);

    /* if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan
     * to never work again. For this reason SCAN_BIT is cleared too */
    const bool        is_connected_to_wifi = wifi_manager_is_connected_to_wifi();
    const EventBits_t event_bits           = xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);

    update_reason_code_e update_reason_code = UPDATE_LOST_CONNECTION;
    if (0 != (event_bits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT))
    {
        LOG_INFO("event_bits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT");
        /* there are no retries when it's a user requested connection by design. This avoids a user
         * hanging too much in case they typed a wrong password for instance. Here we simply clear the
         * request bit and move on */
        xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

        update_reason_code = UPDATE_FAILED_ATTEMPT;
    }
    else if (0 != (event_bits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT))
    {
        LOG_INFO("User manually requested a disconnect so the lost connection is a normal event");
        xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

        update_reason_code = UPDATE_USER_DISCONNECT;
    }
    else
    {
        LOG_INFO("lost connection");
        update_reason_code = UPDATE_LOST_CONNECTION;
        wifiman_msg_send_cmd_connect_sta(CONNECTION_REQUEST_AUTO_RECONNECT);
    }
    const wifi_ssid_t ssid = wifi_sta_config_get_ssid();
    wifi_manager_update_network_connection_info(update_reason_code, &ssid, NULL, NULL);
    if (!is_connected_to_wifi)
    {
        return false;
    }
    return true;
}

static void
wifi_handle_cmd_start_ap(void)
{
    LOG_INFO("MESSAGE: ORDER_START_AP");
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_AP_ACTIVE);
}

static void
wifi_handle_cmd_stop_ap(void)
{
    LOG_INFO("MESSAGE: ORDER_STOP_AP");
    LOG_INFO("Configure WiFi mode: Station");
    const esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "esp_wifi_set_mode failed");
    }
    xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_AP_ACTIVE);
}

static void
wifi_handle_ev_sta_got_ip(const wifiman_msg_param_t *const p_param)
{
    (void)p_param;
    LOG_INFO("MESSAGE: EVENT_STA_GOT_IP");

    const EventBits_t event_bits = xEventGroupClearBits(
        g_p_wifi_manager_event_group,
        WIFI_MANAGER_REQUEST_STA_CONNECT_BIT | WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);

    /* save wifi config in NVS if it wasn't a restored of a connection */
    if (0 == (event_bits & WIFI_MANAGER_REQUEST_RESTORE_STA_BIT))
    {
        wifi_sta_config_save();
    }

    esp_netif_ip_info_t ip_info = { 0 };

    esp_netif_t *const p_netif_sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    const esp_err_t    err         = esp_netif_get_ip_info(p_netif_sta, &ip_info);
    if (ESP_OK == err)
    {
        /* refresh JSON with the new IP */
        const wifi_ssid_t ssid = wifi_sta_config_get_ssid();

        esp_ip4_addr_t           dhcp_ip = { 0 };
        const struct dhcp *const p_dhcp  = netif_dhcp_data((struct netif *)esp_netif_get_netif_impl(p_netif_sta));
        if (NULL != p_dhcp)
        {
            dhcp_ip.addr = p_dhcp->server_ip_addr.u_addr.ip4.addr;
        }
        wifi_manager_update_network_connection_info(
            UPDATE_CONNECTION_OK,
            &ssid,
            &ip_info,
            (NULL != p_dhcp) ? &dhcp_ip : NULL);
    }
    else
    {
        LOG_ERR_ESP(err, "%s failed", "esp_netif_get_ip_info");
    }
}

static void
wifi_handle_ev_ap_sta_connected(void)
{
    LOG_INFO("MESSAGE: EVENT_AP_STA_CONNECTED");
    xEventGroupClearBits(g_p_wifi_manager_event_group, WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT);
    xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
    http_server_on_ap_sta_connected();
    wifi_callback_on_ap_sta_connected();
    if (!wifi_manager_is_connected_to_wifi())
    {
        dns_server_start();
    }
}

static void
wifi_handle_ev_ap_sta_disconnected(void)
{
    LOG_INFO("MESSAGE: EVENT_AP_STA_DISCONNECTED");
    xEventGroupClearBits(
        g_p_wifi_manager_event_group,
        WIFI_MANAGER_AP_STA_CONNECTED_BIT | WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT);
    http_server_on_ap_sta_disconnected();
    wifi_callback_on_ap_sta_disconnected();
    dns_server_stop();
}

static void
wifi_handle_ev_ap_sta_ip_assigned(void)
{
    LOG_INFO("MESSAGE: EVENT_AP_STA_IP_ASSIGNED");
    xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT);
    http_server_on_ap_sta_ip_assigned();
}

static void
wifi_handle_cmd_disconnect_eth(void)
{
    LOG_INFO("MESSAGE: ORDER_DISCONNECT_ETH");
    wifi_callback_on_disconnect_eth_cmd();
}

static void
wifi_handle_cmd_disconnect_sta(void)
{
    LOG_INFO("MESSAGE: ORDER_DISCONNECT_STA");

    /* precise this is coming from a user request */
    xEventGroupSetBits(g_p_wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

    const esp_err_t err = esp_wifi_disconnect();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_wifi_disconnect");
    }
    wifi_callback_on_disconnect_sta_cmd();
}

static void
wifi_handle_ev_scan_next(void)
{
    const wifi_manager_scan_info_t *const p_scan_info = &g_wifi_scan_info;
    /* wifi scanner config */
    const wifi_scan_config_t scan_config = {
        .ssid        = NULL,
        .bssid       = NULL,
        .channel     = p_scan_info->cur_chan,
        .show_hidden = true,
        .scan_type   = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time   = {
            .active  = {
                .min = 0,
                .max = 100,
            },
            .passive = 0,
        },
    };

    LOG_INFO("Start scanning WiFi channel %u", p_scan_info->cur_chan);
    const esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
    // sometimes when connecting to a network, a scan is started at the same time and then the scan
    // will fail that's fine because we already have a network to connect and we don't need new scan
    // results
    if (0 != ret)
    {
        LOG_WARN("EVENT_SCAN_NEXT: scan start return: %d", ret);
        wifi_manager_lock();
        wifi_manger_notify_scan_done();
        wifi_manager_unlock();
    }
}

static void
wifi_handle_ev_scan_done(void)
{
    wifi_manager_scan_info_t *const p_scan_info = &g_wifi_scan_info;
    LOG_DBG("MESSAGE: EVENT_SCAN_DONE: channel=%u", (printf_uint_t)p_scan_info->cur_chan);

    wifi_manager_lock();

    uint16_t        wifi_ap_num = MAX_AP_NUM;
    const esp_err_t err         = esp_wifi_scan_get_ap_records(&wifi_ap_num, &g_wifi_ap_records[MAX_AP_NUM]);
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(
            err,
            "MESSAGE: EVENT_SCAN_DONE: channel=%u: esp_wifi_scan_get_ap_records failed",
            (printf_uint_t)p_scan_info->cur_chan);
        wifi_manger_notify_scan_done();
        wifi_manager_unlock();
        return;
    }

    LOG_INFO(
        "EVENT_SCAN_DONE: found %u Wi-Fi APs on channel %u",
        (printf_uint_t)wifi_ap_num,
        (printf_int_t)p_scan_info->cur_chan);

    /* Will remove the duplicate SSIDs from the list and update ap_num */
    p_scan_info->num_access_points = ap_list_filter_unique(
        g_wifi_ap_records,
        sizeof(g_wifi_ap_records) / sizeof(g_wifi_ap_records[0]));

    /* Put SSID's with the highest quality to the beginning of the list */
    ap_list_sort_by_rssi(g_wifi_ap_records, p_scan_info->num_access_points);
    g_wifi_ap_num = p_scan_info->num_access_points;

    if (p_scan_info->num_access_points >= MAX_AP_NUM)
    {
        p_scan_info->num_access_points = MAX_AP_NUM;
    }
    if (wifi_scan_next(&g_wifi_scan_info))
    {
        LOG_INFO("EVENT_SCAN_DONE: scanning finished");
        wifi_manger_notify_scan_done();
        wifi_manager_unlock();
    }
    else
    {
        wifi_manager_unlock();
    }
}

static void
wifi_manager_wdt_task_reset(void)
{
    const esp_err_t err = esp_task_wdt_reset();
    if (ESP_OK != err)
    {
        LOG_ERR_ESP(err, "%s failed", "esp_task_wdt_reset");
    }
}

bool
wifi_manager_recv_and_handle_msg(void)
{
    bool          flag_terminate = false;
    queue_message msg            = { 0 };
    if (!wifiman_msg_recv(&msg))
    {
        LOG_ERR("%s failed", "wifiman_msg_recv");
        /**
         * wifiman_msg_recv calls xQueueReceive with infinite timeout,
         * so it should never return false and we should never get here,
         * but as a safety precaution to prevent 100% CPU usage we can sleep for a while to give time other threads.
         */
        const uint32_t delay_ms = 100U;
        vTaskDelay(delay_ms / portTICK_PERIOD_MS);
        return flag_terminate;
    }
    bool flag_do_not_call_cb = false;
    switch (msg.code)
    {
        case ORDER_STOP_AND_DESTROY:
            LOG_INFO("Got msg: ORDER_STOP_AND_DESTROY");
            flag_terminate = true;
            break;
        case ORDER_START_WIFI_SCAN:
            wifi_handle_cmd_start_wifi_scan();
            break;
        case ORDER_CONNECT_ETH:
            wifi_handle_cmd_connect_eth();
            break;
        case ORDER_CONNECT_STA:
            wifi_handle_cmd_connect_sta(&msg.msg_param);
            break;
        case ORDER_DISCONNECT_ETH:
            wifi_handle_cmd_disconnect_eth();
            break;
        case ORDER_DISCONNECT_STA:
            wifi_handle_cmd_disconnect_sta();
            break;
        case ORDER_START_AP:
            wifi_handle_cmd_start_ap();
            break;
        case ORDER_STOP_AP:
            wifi_handle_cmd_stop_ap();
            break;

        case EVENT_STA_DISCONNECTED:
            if (!wifi_handle_ev_sta_disconnected(&msg.msg_param))
            {
                flag_do_not_call_cb = true;
            }
            break;
        case EVENT_SCAN_NEXT:
            wifi_handle_ev_scan_next();
            break;
        case EVENT_SCAN_DONE:
            wifi_handle_ev_scan_done();
            break;
        case EVENT_STA_GOT_IP:
            wifi_handle_ev_sta_got_ip(&msg.msg_param);
            break;
        case EVENT_AP_STA_CONNECTED:
            wifi_handle_ev_ap_sta_connected();
            break;
        case EVENT_AP_STA_DISCONNECTED:
            wifi_handle_ev_ap_sta_disconnected();
            break;
        case EVENT_AP_STA_IP_ASSIGNED:
            wifi_handle_ev_ap_sta_ip_assigned();
            break;
        case ORDER_TASK_WATCHDOG_FEED:
            wifi_manager_wdt_task_reset();
            break;
        default:
            break;
    }
    if ((NULL != g_wifi_cb_ptr_arr[msg.code]) && (!flag_do_not_call_cb))
    {
        (*g_wifi_cb_ptr_arr[msg.code])(NULL);
    }
    return flag_terminate;
}

void
wifi_manager_set_callback(const message_code_e message_code, wifi_manager_cb_ptr p_callback)
{
    if (message_code < MESSAGE_CODE_COUNT)
    {
        g_wifi_cb_ptr_arr[message_code] = p_callback;
    }
}

const char *
wifi_manager_generate_access_points_json(void)
{
    return json_access_points_generate(g_wifi_ap_records, g_wifi_ap_num);
}
