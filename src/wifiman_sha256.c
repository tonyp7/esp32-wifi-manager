/**
 * @file wifiman_sha256.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifiman_sha256.h"
#include "mbedtls/sha256.h"
#include "str_buf.h"

wifiman_sha256_digest_hex_str_t
wifiman_sha256_hex_str(const wifiman_sha256_digest_t *const p_digest)
{
    wifiman_sha256_digest_hex_str_t digest_str     = { 0 };
    str_buf_t                       str_buf_digest = STR_BUF_INIT(digest_str.buf, sizeof(digest_str.buf));
    str_buf_bin_to_hex(&str_buf_digest, p_digest->buf, sizeof(p_digest->buf));
    return digest_str;
}

bool
wifiman_sha256_calc(const void *const p_buf, const size_t buf_size, wifiman_sha256_digest_t *const p_digest)
{
    mbedtls_sha256_context ctx = { 0 };
    mbedtls_sha256_init(&ctx);
    const int flag_is_224 = 0;
    if (0 != mbedtls_sha256_starts_ret(&ctx, flag_is_224))
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    if (0 != mbedtls_sha256_update_ret(&ctx, p_buf, buf_size))
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    if (0 != mbedtls_sha256_finish_ret(&ctx, p_digest->buf))
    {
        mbedtls_sha256_free(&ctx);
        return false;
    }
    mbedtls_sha256_free(&ctx);
    return true;
}

wifiman_sha256_digest_hex_str_t
wifiman_sha256_calc_hex_str(const void *const p_buf, const size_t buf_size)
{
    wifiman_sha256_digest_t digest = { 0 };
    if (!wifiman_sha256_calc(p_buf, buf_size, &digest))
    {
        // return empty string
        const wifiman_sha256_digest_hex_str_t digest_hex_str = { { '\0' } };
        return digest_hex_str;
    }
    return wifiman_sha256_hex_str(&digest);
}

bool
wifiman_sha256_is_empty_digest_hex_str(const wifiman_sha256_digest_hex_str_t *const p_digest_hex_str)
{
    if ('\0' == p_digest_hex_str->buf[0])
    {
        return true;
    }
    return false;
}
