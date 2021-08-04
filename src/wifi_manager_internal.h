/**
 * @file wifi_manager_internal.h
 * @author TheSomeMan
 * @date 2020-10-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_WIFI_MANAGER_INTERNAL_H
#define RUUVI_WIFI_MANAGER_INTERNAL_H

#include "wifi_manager_defs.h"
#include "http_req.h"
#include "os_wrapper_types.h"

#ifdef __cplusplus
extern "C" {
#endif

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
wifi_manager_cb_on_user_req(const http_server_user_req_code_e req_code);

http_server_resp_t
wifi_manager_cb_on_http_get(
    const char *const               p_path,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth);

http_server_resp_t
wifi_manager_cb_on_http_post(const char *p_path, const http_req_body_t http_body);

http_server_resp_t
wifi_manager_cb_on_http_delete(
    const char *                    p_path,
    const bool                      flag_access_from_lan,
    const http_server_resp_t *const p_resp_auth);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_WIFI_MANAGER_INTERNAL_H
