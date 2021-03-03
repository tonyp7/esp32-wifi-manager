/**
 * @file sta_ip_safe.h
 * @author TheSomeMan
 * @date 2020-08-26
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_ESP32_WIFI_MANAGER_STA_IP_SAFE_H
#define RUUVI_ESP32_WIFI_MANAGER_STA_IP_SAFE_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "sta_ip.h"

#if !defined(RUUVI_TESTS_STA_IP_SAFE)
#define RUUVI_TESTS_STA_IP_SAFE (0)
#endif

#if RUUVI_TESTS_STA_IP_SAFE
#define STA_IP_SAFE_STATIC
#else
#define STA_IP_SAFE_STATIC static
#endif

#if RUUVI_TESTS_STA_IP_SAFE
#include "freertos/semphr.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize sta_ip_safe library.
 */
void
sta_ip_safe_init(void);

/**
 * @brief Deinit sta_ip_safe library.
 */
void
sta_ip_safe_deinit(void);

/**
 * @brief Gets the string representation of the Station IP address, e.g.: "192.168.1.1"
 * @note This function is thread safe.
 * @return @def sta_ip_string_t
 */
sta_ip_string_t
sta_ip_safe_get(void);

/**
 * @brief Update string representation of the Station IP address
 * @note This function is thread safe.
 * @param ip - IPv4 address (@def sta_ip_address_t)
 */
void
sta_ip_safe_set(const sta_ip_address_t ip);

/**
 * @brief Reset string representation of the Station IP address
 * @note This function is thread safe.
 */
void
sta_ip_safe_reset(void);

/**
 * @brief Convert string representation of IP-address to @def sta_ip_address_t
 * @param p_ip_addr_str - ptr to string representation of IP-address
 * @return @def sta_ip_address_t
 */
sta_ip_address_t
sta_ip_safe_conv_str_to_ip(const char *p_ip_addr_str);

#if RUUVI_TESTS_STA_IP_SAFE

SemaphoreHandle_t
sta_ip_safe_mutex_get(void);

void
sta_ip_safe_lock(void);

void
sta_ip_safe_unlock(void);

#endif

#ifdef __cplusplus
}
#endif

#endif // RUUVI_ESP32_WIFI_MANAGER_STA_IP_SAFE_H
