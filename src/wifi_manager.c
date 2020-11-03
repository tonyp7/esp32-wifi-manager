/*
Copyright (c) 2017-2019 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file wifi_manager.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include "wifi_manager.h"
#include "wifi_manager_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"

#include "json.h"
#include "http_server.h"
#include "tcpip_adapter.h"
#include "dns_server.h"
#include "json_network_info.h"
#include "json_access_points.h"
#include "../../main/includes/ethernet.h"
#include "sta_ip_safe.h"
#include "ap_ssid.h"

#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

/* objects used to manipulate the main queue of events */
QueueHandle_t wifi_manager_queue;

SemaphoreHandle_t wifi_manager_json_mutex = NULL;

uint16_t          ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records;

wifi_config_t *wifi_manager_config_sta = NULL;

void (**cb_ptr_arr)(void *) = NULL;

static wifi_manager_http_callback_t   g_wifi_cb_on_http_get;
static wifi_manager_http_cb_on_post_t g_wifi_cb_on_http_post;
static wifi_manager_http_callback_t   g_wifi_cb_on_http_delete;

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifi_manager";

/* @brief task handle for the main wifi_manager task */
static TaskHandle_t task_wifi_manager = NULL;

static const struct wifi_settings_t wifi_default_settings = {
    .ap_ssid        = DEFAULT_AP_SSID,
    .ap_pwd         = DEFAULT_AP_PASSWORD,
    .ap_channel     = DEFAULT_AP_CHANNEL,
    .ap_ssid_hidden = DEFAULT_AP_SSID_HIDDEN,
    .ap_bandwidth   = DEFAULT_AP_BANDWIDTH,
    .sta_only       = DEFAULT_STA_ONLY,
    .sta_power_save = DEFAULT_STA_POWER_SAVE,
    .sta_static_ip  = 0,
};

/**
 * The actual WiFi settings in use
 */
struct wifi_settings_t wifi_settings = {
    .ap_ssid        = DEFAULT_AP_SSID,
    .ap_pwd         = DEFAULT_AP_PASSWORD,
    .ap_channel     = DEFAULT_AP_CHANNEL,
    .ap_ssid_hidden = DEFAULT_AP_SSID_HIDDEN,
    .ap_bandwidth   = DEFAULT_AP_BANDWIDTH,
    .sta_only       = DEFAULT_STA_ONLY,
    .sta_power_save = DEFAULT_STA_POWER_SAVE,
    .sta_static_ip  = 0,
};

const char wifi_manager_nvs_namespace[] = "espwifimgr";

EventGroupHandle_t wifi_manager_event_group;

/* @brief indicate that the ESP32 is currently connected. */
const int WIFI_MANAGER_WIFI_CONNECTED_BIT = BIT0;

const int WIFI_MANAGER_AP_STA_CONNECTED_BIT = BIT1;

/* @brief Set automatically once the SoftAP is started */
const int WIFI_MANAGER_AP_STARTED_BIT = BIT2;

/* @brief When set, means a client requested to connect to an access point.*/
const int WIFI_MANAGER_REQUEST_STA_CONNECT_BIT = BIT3;

/* @brief This bit is set automatically as soon as a connection was lost */
const int WIFI_MANAGER_STA_DISCONNECT_BIT = BIT4;

/* @brief When set, means the wifi manager attempts to restore a previously saved connection at startup. */
const int WIFI_MANAGER_REQUEST_RESTORE_STA_BIT = BIT5;

/* @brief When set, means a client requested to disconnect from currently connected AP. */
const int WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT = BIT6;

/* @brief When set, means a scan is in progress */
const int WIFI_MANAGER_SCAN_BIT = BIT7;

/* @brief When set, means user requested for a disconnect */
const int WIFI_MANAGER_REQUEST_DISCONNECT_BIT = BIT8;

void
wifi_manager_scan_async()
{
    wifi_manager_send_message(ORDER_START_WIFI_SCAN, NULL);
}

void
wifi_manager_disconnect_async()
{
    wifi_manager_send_message(ORDER_DISCONNECT_STA, NULL);
    // xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT); TODO: delete
}

void
wifi_manager_start(
    const WiFiAntConfig_t *        pWiFiAntConfig,
    wifi_manager_http_callback_t   cb_on_http_get,
    wifi_manager_http_cb_on_post_t cb_on_http_post,
    wifi_manager_http_callback_t   cb_on_http_delete)
{
    /* disable the default wifi logging */
    // esp_log_level_set(TAG, ESP_LOG_DEBUG);

    /* initialize flash memory */
    // nvs_flash_init();

    g_wifi_cb_on_http_get    = cb_on_http_get;
    g_wifi_cb_on_http_post   = cb_on_http_post;
    g_wifi_cb_on_http_delete = cb_on_http_delete;

    /* memory allocation */
    wifi_manager_queue      = xQueueCreate(3, sizeof(queue_message));
    wifi_manager_json_mutex = xSemaphoreCreateMutex();

    accessp_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * MAX_AP_NUM);
    // TODO: check accessp_records for NULL

    ESP_ERROR_CHECK(json_access_points_init());

    json_network_info_init();

    wifi_manager_config_sta = (wifi_config_t *)malloc(sizeof(wifi_config_t));
    // TODO: check wifi_manager_config_sta for NULL
    memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

    memset(&wifi_settings.sta_static_ip_config, 0x00, sizeof(tcpip_adapter_ip_info_t));
    cb_ptr_arr = malloc(sizeof(sizeof(void (*)(void *))) * MESSAGE_CODE_COUNT);
    // TODO: check cb_ptr_arr for NULL
    for (int i = 0; i < MESSAGE_CODE_COUNT; i++)
    {
        cb_ptr_arr[i] = NULL;
    }
    sta_ip_safe_init();
    wifi_manager_event_group = xEventGroupCreate();

    /* start wifi manager task */
    xTaskCreate(
        &wifi_manager,
        "wifi_manager",
        4096,
        (void *)pWiFiAntConfig,
        WIFI_MANAGER_TASK_PRIORITY,
        &task_wifi_manager);
}

http_server_resp_t
wifi_manager_cb_on_http_get(const char *path)
{
    if (NULL == g_wifi_cb_on_http_get)
    {
        return http_server_resp_404();
    }
    return g_wifi_cb_on_http_get(path);
}

http_server_resp_t
wifi_manager_cb_on_http_post(const char *path, const char *body)
{
    if (NULL == g_wifi_cb_on_http_post)
    {
        return http_server_resp_404();
    }
    return g_wifi_cb_on_http_post(path, body);
}

http_server_resp_t
wifi_manager_cb_on_http_delete(const char *path)
{
    if (NULL == g_wifi_cb_on_http_delete)
    {
        return http_server_resp_404();
    }
    return g_wifi_cb_on_http_delete(path);
}

esp_err_t
wifi_manager_clear_sta_config(void)
{
    nvs_handle handle  = 0;
    esp_err_t  esp_err = 0;
    ESP_LOGI(TAG, "About to clear config in flash");

    esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
    if (esp_err != ESP_OK)
        return esp_err;

    esp_err = nvs_set_blob(handle, "ssid", "", 32);
    if (esp_err != ESP_OK)
        return esp_err;

    esp_err = nvs_set_blob(handle, "password", "", 64);
    if (esp_err != ESP_OK)
        return esp_err;

    esp_err = nvs_set_blob(handle, "settings", &wifi_default_settings, sizeof(wifi_default_settings));
    if (esp_err != ESP_OK)
        return esp_err;

    esp_err = nvs_commit(handle);
    if (esp_err != ESP_OK)
        return esp_err;

    nvs_close(handle);
    return ESP_OK;
}

esp_err_t
wifi_manager_save_sta_config()
{
    nvs_handle handle;
    esp_err_t  esp_err;
    ESP_LOGI(TAG, "About to save config to flash");

    if (wifi_manager_config_sta)
    {

        esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
        if (esp_err != ESP_OK)
            return esp_err;

        esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, 32);
        if (esp_err != ESP_OK)
            return esp_err;

        esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, 64);
        if (esp_err != ESP_OK)
            return esp_err;

        esp_err = nvs_set_blob(handle, "settings", &wifi_settings, sizeof(wifi_settings));
        if (esp_err != ESP_OK)
            return esp_err;

        esp_err = nvs_commit(handle);
        if (esp_err != ESP_OK)
            return esp_err;

        nvs_close(handle);

        ESP_LOGD(
            TAG,
            "wifi_manager_wrote wifi_sta_config: ssid:%s password:%s",
            wifi_manager_config_sta->sta.ssid,
            wifi_manager_config_sta->sta.password);
        ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_ssid: %s", wifi_settings.ap_ssid);
        ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_pwd: %s", wifi_settings.ap_pwd);
        ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_channel: %i", wifi_settings.ap_channel);
        ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_hidden (1 = yes): %i", wifi_settings.ap_ssid_hidden);
        ESP_LOGD(
            TAG,
            "wifi_manager_wrote wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i",
            wifi_settings.ap_bandwidth);
        ESP_LOGD(
            TAG,
            "wifi_manager_wrote wifi_settings: sta_only (0 = APSTA, 1 = STA when connected): %i",
            wifi_settings.sta_only);
        ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_power_save (1 = yes): %i", wifi_settings.sta_power_save);
        ESP_LOGD(
            TAG,
            "wifi_manager_wrote wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip): %i",
            wifi_settings.sta_static_ip);
        ESP_LOGD(
            TAG,
            "wifi_manager_wrote wifi_settings: sta_ip_addr: %s",
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.ip));
        ESP_LOGD(
            TAG,
            "wifi_manager_wrote wifi_settings: sta_gw_addr: %s",
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.gw));
        ESP_LOGD(
            TAG,
            "wifi_manager_wrote wifi_settings: sta_netmask: %s",
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.netmask));
    }
    return ESP_OK;
}

bool
wifi_manager_fetch_wifi_sta_config()
{

    nvs_handle handle;
    esp_err_t  esp_err;
    if (nvs_open(wifi_manager_nvs_namespace, NVS_READONLY, &handle) == ESP_OK)
    {

        if (wifi_manager_config_sta == NULL)
        {
            wifi_manager_config_sta = (wifi_config_t *)malloc(sizeof(wifi_config_t));
        }
        memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

        // memset(&wifi_settings, 0x00, sizeof(struct wifi_settings_t));

        /* allocate buffer */
        size_t   sz   = sizeof(wifi_settings);
        uint8_t *buff = (uint8_t *)malloc(sizeof(uint8_t) * sz);
        memset(buff, 0x00, sizeof(sz));

        /* ssid */
        sz      = sizeof(wifi_manager_config_sta->sta.ssid);
        esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
        if (esp_err != ESP_OK)
        {
            free(buff);
            return false;
        }
        memcpy(wifi_manager_config_sta->sta.ssid, buff, sz);

        /* password */
        sz      = sizeof(wifi_manager_config_sta->sta.password);
        esp_err = nvs_get_blob(handle, "password", buff, &sz);
        if (esp_err != ESP_OK)
        {
            free(buff);
            return false;
        }
        memcpy(wifi_manager_config_sta->sta.password, buff, sz);
        /* memcpy(wifi_manager_config_sta->sta.password, "lewrong", strlen("lewrong")); this is debug to force a wrong
         * password event. ignore! */

        /* settings */
        sz      = sizeof(wifi_settings);
        esp_err = nvs_get_blob(handle, "settings", buff, &sz);
        if (esp_err != ESP_OK)
        {
            free(buff);
            return false;
        }
        memcpy(&wifi_settings, buff, sz);

        free(buff);
        nvs_close(handle);

        ESP_LOGI(
            TAG,
            "wifi_manager_fetch_wifi_sta_config: ssid:%s password:%s",
            wifi_manager_config_sta->sta.ssid,
            wifi_manager_config_sta->sta.password);
        ESP_LOGI(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_ssid:%s", wifi_settings.ap_ssid);
        ESP_LOGI(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_pwd:%s", wifi_settings.ap_pwd);
        ESP_LOGI(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_channel:%i", wifi_settings.ap_channel);
        ESP_LOGI(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_hidden (1 = yes):%i", wifi_settings.ap_ssid_hidden);
        ESP_LOGI(
            TAG,
            "wifi_manager_fetch_wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz)%i",
            wifi_settings.ap_bandwidth);
        ESP_LOGI(
            TAG,
            "wifi_manager_fetch_wifi_settings: sta_only (0 = APSTA, 1 = STA when connected):%i",
            wifi_settings.sta_only);
        ESP_LOGI(TAG, "wifi_manager_fetch_wifi_settings: sta_power_save (1 = yes):%i", wifi_settings.sta_power_save);
        ESP_LOGI(
            TAG,
            "wifi_manager_fetch_wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip):%i",
            wifi_settings.sta_static_ip);
        ESP_LOGI(
            TAG,
            "wifi_manager_fetch_wifi_settings: sta_static_ip_config: IP: %s , GW: %s , Mask: %s",
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.ip),
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.gw),
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.netmask));
        ESP_LOGI(
            TAG,
            "wifi_manager_fetch_wifi_settings: sta_ip_addr: %s",
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.ip));
        ESP_LOGI(
            TAG,
            "wifi_manager_fetch_wifi_settings: sta_gw_addr: %s",
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.gw));
        ESP_LOGI(
            TAG,
            "wifi_manager_fetch_wifi_settings: sta_netmask: %s",
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.netmask));

        return wifi_manager_config_sta->sta.ssid[0] != '\0';
    }
    else
    {
        return false;
    }
}

/**
 * @brief Generates the connection status json: ssid and IP addresses.
 * @note This is not thread-safe and should be called only if wifi_manager_lock_json_buffer call is successful.
 */
void
wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code)
{
    tcpip_adapter_ip_info_t ip_info = { 0 };
    ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
    const char *ssid = wifi_manager_get_wifi_sta_ssid();

    network_info_str_t ip_info_str = { 0 };
    snprintf(ip_info_str.ip, sizeof(ip_info_str.ip), "%s", ip4addr_ntoa(&ip_info.ip));
    snprintf(ip_info_str.netmask, sizeof(ip_info_str.netmask), "%s", ip4addr_ntoa(&ip_info.netmask));
    snprintf(ip_info_str.gw, sizeof(ip_info_str.gw), "%s", ip4addr_ntoa(&ip_info.gw));

    json_network_info_generate(ssid, &ip_info_str, update_reason_code);
}

bool
wifi_manager_lock_json_buffer(TickType_t xTicksToWait)
{
    if (wifi_manager_json_mutex)
    {
        if (xSemaphoreTake(wifi_manager_json_mutex, xTicksToWait) == pdTRUE)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        return false;
    }
}

void
wifi_manager_unlock_json_buffer()
{
    xSemaphoreGive(wifi_manager_json_mutex);
}

void
wifi_manager_event_handler(void *ctx, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_WIFI_READY:
                ESP_LOGI(TAG, "WIFI_EVENT_WIFI_READY");
                break;
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGD(TAG, "WIFI_EVENT_SCAN_DONE");
                xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
                wifi_manager_send_message(EVENT_SCAN_DONE, NULL);
                break;
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGE");
                break;
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
                xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
                wifi_manager_scan_async();
                break;
            case WIFI_EVENT_AP_STOP:
                break;
            case WIFI_EVENT_AP_PROBEREQRECVED:
                break;
            case WIFI_EVENT_AP_STACONNECTED: /* a user disconnected from the SoftAP */
                ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
                xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
                break;
            case WIFI_EVENT_AP_STADISCONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
                xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
                break;
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
                break;
            case WIFI_EVENT_STA_STOP:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");
                /* if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan
                 * to never work again. For this reason SCAN_BIT is cleared too */
                xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_SCAN_BIT);
                wifi_event_sta_disconnected_t *disconnected = (wifi_event_sta_disconnected_t *)event_data;
                /* post disconnect event with reason code */
                wifi_manager_send_message(EVENT_STA_DISCONNECTED, (void *)((uint32_t)disconnected->reason));
                break;
            default:
                break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
                xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
                ip_event_got_ip_t *ipevent = (ip_event_got_ip_t *)event_data;
                wifi_manager_send_message(EVENT_STA_GOT_IP, (void *)((uint32_t)ipevent->ip_info.ip.addr));
                break;
        }
    }
}

wifi_config_t *
wifi_manager_get_wifi_sta_config()
{
    return wifi_manager_config_sta;
}

const char *
wifi_manager_get_wifi_sta_ssid(void)
{
    wifi_config_t *p_wifi_cfg = wifi_manager_get_wifi_sta_config();
    const char *   ssid       = (NULL != p_wifi_cfg) ? (const char *)p_wifi_cfg->sta.ssid : NULL;
    return ssid;
}

void
wifi_manager_connect_async()
{
    /* in order to avoid a false positive on the front end app we need to quickly flush the ip json
     * There'se a risk the front end sees an IP or a password error when in fact
     * it's a remnant from a previous connection
     */
    if (wifi_manager_lock_json_buffer(portMAX_DELAY))
    {
        json_network_info_clear();
        wifi_manager_unlock_json_buffer();
    }
    wifi_manager_send_message(ORDER_CONNECT_STA, (void *)CONNECTION_REQUEST_USER);
}

void
wifi_manager_stop()
{
    ESP_LOGI(TAG, "%s", __func__);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler);

    esp_wifi_disconnect();
    esp_wifi_stop();
    esp_wifi_deinit();

    wifi_manager_destroy();

    tcpip_adapter_stop(TCPIP_ADAPTER_IF_STA);
    tcpip_adapter_stop(TCPIP_ADAPTER_IF_AP);
}

void
wifi_manager_destroy()
{
    ESP_LOGI(TAG, "%s", __func__);
    if (task_wifi_manager)
    {
        vTaskDelete(task_wifi_manager);
        task_wifi_manager = NULL;

        /* heap buffers */
        free(accessp_records);
        accessp_records = NULL;
        json_access_points_deinit();
        json_network_info_deinit();
        sta_ip_safe_deinit();
        if (wifi_manager_config_sta)
        {
            free(wifi_manager_config_sta);
            wifi_manager_config_sta = NULL;
        }

        /* RTOS objects */
        vSemaphoreDelete(wifi_manager_json_mutex);
        wifi_manager_json_mutex = NULL;
        vEventGroupDelete(wifi_manager_event_group);
        wifi_manager_event_group = NULL;
        vQueueDelete(wifi_manager_queue);
        wifi_manager_queue = NULL;
    }
}

void
wifi_manager_filter_unique(wifi_ap_record_t *aplist, uint16_t *aps)
{
    int               total_unique;
    wifi_ap_record_t *first_free;
    total_unique = *aps;

    first_free = NULL;

    for (int i = 0; i < *aps - 1; i++)
    {
        wifi_ap_record_t *ap = &aplist[i];

        /* skip the previously removed APs */
        if (ap->ssid[0] == 0)
            continue;

        /* remove the identical SSID+authmodes */
        for (int j = i + 1; j < *aps; j++)
        {
            wifi_ap_record_t *ap1 = &aplist[j];
            if ((strcmp((const char *)ap->ssid, (const char *)ap1->ssid) == 0) && (ap->authmode == ap1->authmode))
            { /* same SSID, different auth mode is skipped */
                /* save the rssi for the display */
                if ((ap1->rssi) > (ap->rssi))
                    ap->rssi = ap1->rssi;
                /* clearing the record */
                memset(ap1, 0, sizeof(wifi_ap_record_t));
            }
        }
    }
    /* reorder the list so APs follow each other in the list */
    for (int i = 0; i < *aps; i++)
    {
        wifi_ap_record_t *ap = &aplist[i];
        /* skipping all that has no name */
        if (ap->ssid[0] == 0)
        {
            /* mark the first free slot */
            if (first_free == NULL)
                first_free = ap;
            total_unique--;
            continue;
        }
        if (first_free != NULL)
        {
            memcpy(first_free, ap, sizeof(wifi_ap_record_t));
            memset(ap, 0, sizeof(wifi_ap_record_t));
            /* find the next free slot */
            for (int j = 0; j < *aps; j++)
            {
                if (aplist[j].ssid[0] == 0)
                {
                    first_free = &aplist[j];
                    break;
                }
            }
        }
    }
    /* update the length of the list */
    *aps = total_unique;
}

BaseType_t
wifi_manager_send_message_to_front(message_code_t code, void *param)
{
    queue_message msg;
    msg.code  = code;
    msg.param = param;
    return xQueueSendToFront(wifi_manager_queue, &msg, portMAX_DELAY);
}

BaseType_t
wifi_manager_send_message(message_code_t code, void *param)
{
    queue_message msg;
    msg.code  = code;
    msg.param = param;
    return xQueueSend(wifi_manager_queue, &msg, portMAX_DELAY);
}

void
wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void *))
{
    if (cb_ptr_arr && message_code < MESSAGE_CODE_COUNT)
    {
        cb_ptr_arr[message_code] = func_ptr;
    }
}

void
wifi_manager(void *pvParameters)
{
    const WiFiAntConfig_t *pWiFiAntConfig = pvParameters;
    queue_message          msg;
    BaseType_t             xStatus;
    EventBits_t            uxBits;
    // uint8_t	retries = 0;

    /* initialize the tcp stack */
    tcpip_adapter_init();

    /* event handler and event group for the wifi driver */
    wifi_manager_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_manager_event_handler, NULL));

    /* wifi scanner config */
    wifi_scan_config_t scan_config = {
        .ssid        = 0,
        .bssid       = 0,
        .channel     = 0,
        .show_hidden = true,
    };

    /* default wifi config */
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    if (NULL != pWiFiAntConfig)
    {
        esp_err_t err = esp_wifi_set_ant_gpio(&pWiFiAntConfig->wifiAntGpioConfig);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "esp_wifi_set_ant_gpio failed, res=%d", err);
        }
        err = esp_wifi_set_ant(&pWiFiAntConfig->wifiAntConfig);
        if (ESP_OK != err)
        {
            ESP_LOGE(TAG, "esp_wifi_set_ant failed, res=%d", err);
        }
    }
    /* SoftAP - Wifi Access Point configuration setup */
    tcpip_adapter_ip_info_t info;
    memset(&info, 0x00, sizeof(info));
    wifi_config_t ap_config = {
        .ap = {
            .ssid_len        = 0,
            .channel         = wifi_settings.ap_channel,
            .authmode        = WIFI_AUTH_WPA2_PSK,
            .ssid_hidden     = wifi_settings.ap_ssid_hidden,
            .max_connection  = DEFAULT_AP_MAX_CONNECTIONS,
            .beacon_interval = DEFAULT_AP_BEACON_INTERVAL,
        },
    };

    {
        ap_mac_t ap_mac = { 0 };
        ESP_ERROR_CHECK(esp_wifi_get_mac(ESP_IF_WIFI_AP, &ap_mac.mac[0]));
        ap_ssid_generate(
            (char *)&ap_config.ap.ssid[0],
            sizeof(ap_config.ap.ssid),
            (const char *)&wifi_settings.ap_ssid[0],
            &ap_mac);
    }
    snprintf((char *)&ap_config.ap.password[0], sizeof(ap_config.ap.password), "%s", wifi_settings.ap_pwd);

    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP)); /* stop AP DHCP server */
    inet_pton(AF_INET, DEFAULT_AP_IP, &info.ip);                    /* access point is on a static IP */
    inet_pton(AF_INET, DEFAULT_AP_GATEWAY, &info.gw);
    inet_pton(AF_INET, DEFAULT_AP_NETMASK, &info.netmask);
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP)); /* start AP DHCP server */

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, wifi_settings.ap_bandwidth));
    ESP_ERROR_CHECK(esp_wifi_set_ps(wifi_settings.sta_power_save));

    /* STA - Wifi Station configuration setup */
    tcpip_adapter_dhcp_status_t status;
    if (wifi_settings.sta_static_ip)
    {
        ESP_LOGI(
            TAG,
            "Assigning static ip to STA interface. IP: %s , GW: %s , Mask: %s",
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.ip),
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.gw),
            ip4addr_ntoa(&wifi_settings.sta_static_ip_config.netmask));

        /* stop DHCP client*/
        ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));
        /* assign a static IP to the STA network interface */
        ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &wifi_settings.sta_static_ip_config));
    }
    else
    {
        /* start DHCP client if not started*/
        ESP_LOGI(TAG, "wifi_manager: Start DHCP client for STA interface. If not already running");
        ESP_ERROR_CHECK(tcpip_adapter_dhcpc_get_status(TCPIP_ADAPTER_IF_STA, &status));
        if (status != TCPIP_ADAPTER_DHCP_STARTED)
            ESP_ERROR_CHECK(tcpip_adapter_dhcpc_start(TCPIP_ADAPTER_IF_STA));
    }

    /* by default the mode is STA because wifi_manager will not start the access point unless it has to! */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* enqueue first event: load previous config */
    wifi_manager_send_message(ORDER_LOAD_AND_RESTORE_STA, NULL);

    /* main processing loop */
    for (;;)
    {
        xStatus = xQueueReceive(wifi_manager_queue, &msg, portMAX_DELAY);

        if (xStatus == pdPASS)
        {
            switch (msg.code)
            {

                case EVENT_SCAN_DONE:
                    ESP_LOGD(TAG, "MESSAGE: EVENT_SCAN_DONE");
                    /* As input param, it stores max AP number ap_records can hold. As output param, it receives the
                     * actual AP number this API returns.
                     * As a consequence, ap_num MUST be reset to MAX_AP_NUM at every scan */
                    ap_num = MAX_AP_NUM;
                    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, accessp_records));
                    /* make sure the http server isn't trying to access the list while it gets refreshed */
                    if (wifi_manager_lock_json_buffer(pdMS_TO_TICKS(1000)))
                    {
                        /* Will remove the duplicate SSIDs from the list and update ap_num */
                        wifi_manager_filter_unique(accessp_records, &ap_num);
                        json_access_points_generate(accessp_records, ap_num);
                        wifi_manager_unlock_json_buffer();
                    }
                    else
                    {
                        ESP_LOGE(TAG, "could not get access to json mutex in wifi_scan");
                    }

                    http_server_start();
                    dns_server_start();

                    /* callback */
                    if (cb_ptr_arr[msg.code])
                        (*cb_ptr_arr[msg.code])(NULL);

                    break;

                case ORDER_START_WIFI_SCAN:
                    ESP_LOGD(TAG, "MESSAGE: ORDER_START_WIFI_SCAN");

                    /* if a scan is already in progress this message is simply ignored thanks to the
                     * WIFI_MANAGER_SCAN_BIT uxBit */
                    uxBits = xEventGroupGetBits(wifi_manager_event_group);
                    if (!(uxBits & WIFI_MANAGER_SCAN_BIT))
                    {
                        xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
                        esp_err_t ret = esp_wifi_scan_start(&scan_config, false);
                        // sometimes when connecting to a network, a scan is started at the same time and then the scan
                        // will fail that's fine because we already have a network to connect and we don't need new scan
                        // results
                        // TODO: maybe fix this in the web page that scanning is stopped when connecting to a network
                        if (ret != 0)
                        {
                            ESP_LOGW(TAG, "scan start return: %d", ret);
                        }
                    }

                    /* callback */
                    if (cb_ptr_arr[msg.code])
                        (*cb_ptr_arr[msg.code])(NULL);

                    break;

                case ORDER_LOAD_AND_RESTORE_STA:
                    ESP_LOGI(TAG, "MESSAGE: ORDER_LOAD_AND_RESTORE_STA");
                    if (wifi_manager_fetch_wifi_sta_config())
                    {
                        ESP_LOGI(TAG, "Saved wifi found on startup. Will attempt to connect.");
                        wifi_manager_send_message(ORDER_CONNECT_STA, (void *)CONNECTION_REQUEST_RESTORE_CONNECTION);
                    }
                    else
                    {
                        /* no wifi saved: start soft AP! This is what should happen during a first run */
                        ESP_LOGI(TAG, "No saved wifi found on startup. Starting access point.");
                        wifi_manager_send_message(ORDER_START_AP, NULL);
                    }

                    /* callback */
                    if (cb_ptr_arr[msg.code])
                        (*cb_ptr_arr[msg.code])(NULL);

                    break;

                case ORDER_CONNECT_STA:
                    ESP_LOGI(TAG, "MESSAGE: ORDER_CONNECT_STA");

                    /* very important: precise that this connection attempt is specifically requested.
                     * Param in that case is a boolean indicating if the request was made automatically
                     * by the wifi_manager.
                     * */
                    if ((BaseType_t)msg.param == CONNECTION_REQUEST_USER)
                    {
                        xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
                    }
                    else if ((BaseType_t)msg.param == CONNECTION_REQUEST_RESTORE_CONNECTION)
                    {
                        xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
                    }

                    uxBits = xEventGroupGetBits(wifi_manager_event_group);
                    if (uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT)
                    {
                        wifi_manager_send_message(ORDER_DISCONNECT_STA, NULL);
                        /* todo: reconnect */
                    }
                    else
                    {
                        /* update config to latest and attempt connection */
                        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, wifi_manager_get_wifi_sta_config()));
                        ESP_ERROR_CHECK(esp_wifi_connect());
                    }

                    /* callback */
                    if (cb_ptr_arr[msg.code])
                        (*cb_ptr_arr[msg.code])(NULL);

                    break;

                case EVENT_STA_DISCONNECTED:
                    ESP_LOGI(TAG, "MESSAGE: EVENT_STA_DISCONNECTED with Reason code: %d", (uint32_t)msg.param);

                    /* this even can be posted in numerous different conditions
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
                     * */

                    /* reset saved sta IP */
                    sta_ip_safe_reset(portMAX_DELAY);

                    uxBits = xEventGroupGetBits(wifi_manager_event_group);
                    if (uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT)
                    {
                        ESP_LOGI(TAG, "uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT");
                        /* there are no retries when it's a user requested connection by design. This avoids a user
                         * hanging too much in case they typed a wrong password for instance. Here we simply clear the
                         * request bit and move on */
                        xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

                        if (wifi_manager_lock_json_buffer(portMAX_DELAY))
                        {
                            wifi_manager_generate_ip_info_json(UPDATE_FAILED_ATTEMPT);
                            wifi_manager_unlock_json_buffer();
                        }
                    }
                    else if (uxBits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT)
                    {
                        ESP_LOGI(TAG, "uxBits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT");
                        /* user manually requested a disconnect so the lost connection is a normal event. Clear the flag
                         * and restart the AP */
                        xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

                        if (wifi_manager_lock_json_buffer(portMAX_DELAY))
                        {
                            wifi_manager_generate_ip_info_json(UPDATE_USER_DISCONNECT);
                            wifi_manager_unlock_json_buffer();
                        }

                        /* erase configuration */
                        if (wifi_manager_config_sta)
                        {
                            memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
                        }

                        /* save NVS memory */
                        wifi_manager_save_sta_config();

                        /* start SoftAP */
                        wifi_manager_send_message(ORDER_START_AP, NULL);
                    }
                    else
                    {
                        ESP_LOGI(TAG, "lost connection");
                        /* lost connection ? */
                        if (wifi_manager_lock_json_buffer(portMAX_DELAY))
                        {
                            wifi_manager_generate_ip_info_json(UPDATE_LOST_CONNECTION);
                            wifi_manager_unlock_json_buffer();
                        }

                        wifi_manager_send_message(ORDER_CONNECT_STA, (void *)CONNECTION_REQUEST_AUTO_RECONNECT);
                    }

                    /* callback */
                    if (cb_ptr_arr[msg.code])
                        (*cb_ptr_arr[msg.code])(NULL);

                    break;

                case ORDER_START_AP:
                    ESP_LOGI(TAG, "MESSAGE: ORDER_START_AP");
                    esp_wifi_set_mode(WIFI_MODE_APSTA);

                    // http_server_start();
                    // dns_server_start();

                    /* callback */
                    if (cb_ptr_arr[msg.code])
                        (*cb_ptr_arr[msg.code])(NULL);

                    break;

                case EVENT_STA_GOT_IP:
                    ESP_LOGI(TAG, "MESSAGE: EVENT_STA_GOT_IP");

                    uxBits = xEventGroupGetBits(wifi_manager_event_group);

                    /* reset connection requests bits -- doesn't matter if it was set or not */
                    xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

                    /* save IP as a string for the HTTP server host */
                    sta_ip_safe_set((uint32_t)(uintptr_t)msg.param, portMAX_DELAY);

                    /* save wifi config in NVS if it wasn't a restored of a connection */
                    if (uxBits & WIFI_MANAGER_REQUEST_RESTORE_STA_BIT)
                    {
                        xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
                    }
                    else
                    {
                        wifi_manager_save_sta_config();
                    }

                    /* refresh JSON with the new IP */
                    if (wifi_manager_lock_json_buffer(portMAX_DELAY))
                    {
                        /* generate the connection info with success */
                        wifi_manager_generate_ip_info_json(UPDATE_CONNECTION_OK);
                        wifi_manager_unlock_json_buffer();
                    }
                    else
                    {
                        abort();
                    }

                    /* bring down DNS hijack */
                    // http_server_stop();
                    dns_server_stop();

                    /* callback */
                    if (cb_ptr_arr[msg.code])
                        (*cb_ptr_arr[msg.code])(NULL);

                    break;

                case ORDER_DISCONNECT_STA:
                    ESP_LOGI(TAG, "MESSAGE: ORDER_DISCONNECT_STA");

                    /* precise this is coming from a user request */
                    xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

                    /* order wifi discconect */
                    ESP_ERROR_CHECK(esp_wifi_disconnect());

                    /* callback */
                    if (cb_ptr_arr[msg.code])
                        (*cb_ptr_arr[msg.code])(NULL);

                    break;

                default:
                    break;

            } /* end of switch/case */
        }     /* end of if status=pdPASS */
    }         /* end of for loop */
    vTaskDelete(NULL);
}
