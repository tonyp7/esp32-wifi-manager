/**
 * @file wifiman_md5.h
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef ESP32_WIFI_MANAGER_MD5_H
#define ESP32_WIFI_MANAGER_MD5_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WIFIMAN_MD5_DIGEST_SIZE (16U)

typedef struct wifiman_md5_digest_t
{
    uint8_t buf[WIFIMAN_MD5_DIGEST_SIZE];
} wifiman_md5_digest_t;

typedef struct wifiman_md5_digest_hex_str_t
{
    char buf[2 * WIFIMAN_MD5_DIGEST_SIZE + 1];
} wifiman_md5_digest_hex_str_t;

bool
wifiman_md5_calc(const void *const p_buf, const size_t buf_size, wifiman_md5_digest_t *const p_digest);

wifiman_md5_digest_hex_str_t
wifiman_md5_hex_str(const wifiman_md5_digest_t *const p_digest);

wifiman_md5_digest_hex_str_t
wifiman_md5_calc_hex_str(const void *const p_buf, const size_t buf_size);

bool
wifiman_md5_is_empty_digest_hex_str(const wifiman_md5_digest_hex_str_t *const p_digest_hex_str);

#ifdef __cplusplus
}
#endif

#endif // ESP32_WIFI_MANAGER_MD5_H
