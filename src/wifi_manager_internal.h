/**
 * @file wifi_manager_internal.h
 * @author TheSomeMan
 * @date 2020-10-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_WIFI_MANAGER_INTERNAL_H
#define RUUVI_WIFI_MANAGER_INTERNAL_H

#include "wifi_manager_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "os_wrapper_types.h"
#include "os_sema.h"
#include "os_timer.h"
#include "wifi_sta_config.h"
#include "http_req.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @brief indicates that wifi_manager is working. */
#define WIFI_MANAGER_IS_WORKING ((uint32_t)(BIT0))

/* @brief indicates that the ESP32 is currently connected. */
#define WIFI_MANAGER_WIFI_CONNECTED_BIT ((uint32_t)(BIT1))

#define WIFI_MANAGER_AP_STA_CONNECTED_BIT   ((uint32_t)(BIT2))
#define WIFI_MANAGER_AP_STA_IP_ASSIGNED_BIT ((uint32_t)(BIT3))

/* @brief Set automatically once the SoftAP is started */
#define WIFI_MANAGER_AP_STARTED_BIT ((uint32_t)(BIT4))

/* @brief When set, means a client requested to connect to an access point.*/
#define WIFI_MANAGER_REQUEST_STA_CONNECT_BIT ((uint32_t)(BIT5))

/* @brief When set, means the wifi manager attempts to restore a previously saved connection at startup. */
#define WIFI_MANAGER_REQUEST_RESTORE_STA_BIT ((uint32_t)(BIT6))

/* @brief When set, means a scan is in progress */
#define WIFI_MANAGER_SCAN_BIT ((uint32_t)(BIT7))

/* @brief When set, means user requested for a disconnect */
#define WIFI_MANAGER_REQUEST_DISCONNECT_BIT ((uint32_t)(BIT8))

/* @brief indicate that the device is currently connected to Ethernet. */
#define WIFI_MANAGER_ETH_CONNECTED_BIT ((uint32_t)(BIT9))

/* @brief indicate that WiFi access point is active. */
#define WIFI_MANAGER_AP_ACTIVE ((uint32_t)(BIT10))

#define WIFI_MANAGER_DELAY_BETWEEN_SCANNING_WIFI_CHANNELS_MS (200U)

#define WIFI_MANAGER_WIFI_COUNTRY_DEFAULT_FIRST_CHANNEL (1U)
#define WIFI_MANAGER_WIFI_COUNTRY_DEFAULT_NUM_CHANNELS  (13U)

typedef struct wifi_manager_antenna_config_t wifi_manager_antenna_config_t;

typedef struct wifi_manager_scan_info_t
{
    uint8_t  first_chan;
    uint8_t  last_chan;
    uint8_t  cur_chan;
    uint16_t num_access_points;
} wifi_manager_scan_info_t;

extern EventGroupHandle_t g_p_wifi_manager_event_group;

void
wifi_manager_init_mutex(void);

/**
 * @brief Tries to lock access to wifi_manager internal structures using mutex
 * (it also locks the access points and connection status json buffers).
 *
 * The HTTP server can try to access the json to serve clients while the wifi manager thread can try
 * to update it.
 * Also, wifi_manager can be asynchronously stopped and HTTP server or other threads
 * can try to call wifi_manager while it is de-initializing.
 *
 * @param ticks_to_wait The time in ticks to wait for the mutex to become available.
 * @return true in success, false otherwise.
 */
bool
wifi_manager_lock_with_timeout(const os_delta_ticks_t ticks_to_wait);

/**
 * @brief Lock the wifi_manager mutex.
 */
void
wifi_manager_lock(void);

/**
 * @brief Releases the wifi_manager mutex.
 */
void
wifi_manager_unlock(void);

void
wifi_manager_task(void);

void
wifi_manager_event_handler(
    ATTR_UNUSED void *     p_ctx,
    const esp_event_base_t p_event_base,
    const int32_t          event_id,
    void *                 p_event_data);

void
wifi_manager_cb_on_user_req(const http_server_user_req_code_e req_code);

http_server_resp_t
wifi_manager_cb_on_http_get(
    const char *const               p_path,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth);

http_server_resp_t
wifi_manager_cb_on_http_post(
    const char *const     p_path,
    const http_req_body_t http_body,
    const bool            flag_access_from_lan);

http_server_resp_t
wifi_manager_cb_on_http_delete(
    const char *const               p_path,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth);

bool
wifi_manager_recv_and_handle_msg(void);

const char *
wifi_manager_generate_access_points_json(void);

bool
wifi_manager_init(
    const bool                                 flag_start_wifi,
    const bool                                 flag_start_ap_only,
    const wifi_ssid_t *const                   p_gw_wifi_ssid,
    const wifi_manager_antenna_config_t *const p_wifi_ant_config,
    const wifi_manager_callbacks_t *const      p_callbacks);

void
wifi_manager_scan_timer_start(void);

void
wifi_callback_on_connect_eth_cmd(void);

void
wifi_callback_on_ap_sta_connected(void);

void
wifi_callback_on_ap_sta_disconnected(void);

void
wifi_callback_on_disconnect_eth_cmd(void);

void
wifi_callback_on_disconnect_sta_cmd(void);

void
wifi_manger_notify_scan_done(void);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_WIFI_MANAGER_INTERNAL_H
