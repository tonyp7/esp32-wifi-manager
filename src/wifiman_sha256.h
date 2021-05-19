/**
 * @file wifiman_sha256.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_SHA256_H
#define ESP32_WIFI_MANAGER_SHA256_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHA256_DIGEST_SIZE (32U)

typedef struct wifiman_sha256_digest_t
{
    uint8_t buf[SHA256_DIGEST_SIZE];
} wifiman_sha256_digest_t;

typedef struct wifiman_sha256_digest_hex_str_t
{
    char buf[2 * SHA256_DIGEST_SIZE + 1];
} wifiman_sha256_digest_hex_str_t;

bool
wifiman_sha256_calc(const void *const p_buf, const size_t buf_size, wifiman_sha256_digest_t *const p_digest);

wifiman_sha256_digest_hex_str_t
wifiman_sha256_hex_str(const wifiman_sha256_digest_t *const p_digest);

wifiman_sha256_digest_hex_str_t
wifiman_sha256_calc_hex_str(const void *const p_buf, const size_t buf_size);

bool
wifiman_sha256_is_empty_digest_hex_str(const wifiman_sha256_digest_hex_str_t *const p_digest_hex_str);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_SHA256_H
