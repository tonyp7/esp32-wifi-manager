/**
 * @file wifi_manager_internal.h
 * @author TheSomeMan
 * @date 2020-10-28
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_WIFI_MANAGER_INTERNAL_H
#define RUUVI_WIFI_MANAGER_INTERNAL_H

#include "wifi_manager_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Tries to get access to json buffer mutex.
 *
 * The HTTP server can try to access the json to serve clients while the wifi manager thread can try
 * to update it. These two tasks are synchronized through a mutex.
 *
 * The mutex is used by both the access point list json and the connection status json.\n
 * These two resources should technically have their own mutex but we lose some flexibility to save
 * on memory.
 *
 * This is a simple wrapper around freeRTOS function xSemaphoreTake.
 *
 * @param xTicksToWait The time in ticks to wait for the semaphore to become available.
 * @return true in success, false otherwise.
 */
bool
wifi_manager_lock_json_buffer(TickType_t xTicksToWait);

/**
 * @brief Releases the json buffer mutex.
 */
void
wifi_manager_unlock_json_buffer(void);

http_server_resp_t
wifi_manager_cb_on_http_get(const char *path);

http_server_resp_t
wifi_manager_cb_on_http_post(const char *path, const char *body);

http_server_resp_t
wifi_manager_cb_on_http_delete(const char *path);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_WIFI_MANAGER_INTERNAL_H
