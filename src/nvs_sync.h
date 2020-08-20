/**
Copyright (c) 2020 Tony Pottier

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

@file nvs_sync.h
@author Tony Pottier
@brief Exposes a simple API to synchronize NVS memory read and writes

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/



#ifndef WIFI_MANAGER_NVS_SYNC_H_INCLUDED
#define WIFI_MANAGER_NVS_SYNC_H_INCLUDED

#include <stdbool.h> /* for type bool */
#include <freertos/FreeRTOS.h> /* for TickType_t */
#include <esp_err.h> /* for esp_err_t */

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Attempts to get hold of the NVS semaphore for a set amount of ticks.
 * @note If you are uncertain about the number of ticks to wait use portMAX_DELAY.
 * @return true on a succesful lock, false otherwise
 */
bool nvs_sync_lock(TickType_t xTicksToWait);


/**
 * @brief Releases the NVS semaphore
 */
void nvs_sync_unlock();


/** 
 * @brief Create the NVS semaphore
 * @return      ESP_OK: success or if the semaphore already exists
 *              ESP_FAIL: failure
 */ 
esp_err_t nvs_sync_create();

/**
 * @brief Frees memory associated with the NVS semaphore 
 * @warning Do not delete a semaphore that has tasks blocked on it (tasks that are in the Blocked state waiting for the semaphore to become available).
 */
void nvs_sync_free();


#ifdef __cplusplus
}
#endif

#endif /* WIFI_MANAGER_NVS_SYNC_H_INCLUDED */