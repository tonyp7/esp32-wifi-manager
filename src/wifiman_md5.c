/**
 * @file wifiman_md5.c
 * @author TheSomeMan
 * @date 2021-05-09
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifiman_md5.h"
#include "mbedtls/md5.h"
#include "str_buf.h"

wifiman_md5_digest_hex_str_t
wifiman_md5_hex_str(const wifiman_md5_digest_t *const p_digest)
{
    wifiman_md5_digest_hex_str_t digest_str     = { 0 };
    str_buf_t                    str_buf_digest = STR_BUF_INIT(digest_str.buf, sizeof(digest_str.buf));
    str_buf_bin_to_hex(&str_buf_digest, p_digest->buf, sizeof(p_digest->buf));
    return digest_str;
}

bool
wifiman_md5_calc(const void *const p_buf, const size_t buf_size, wifiman_md5_digest_t *const p_digest)
{
    mbedtls_md5_context ctx = { 0 };
    mbedtls_md5_init(&ctx);
    if (0 != mbedtls_md5_starts_ret(&ctx))
    {
        mbedtls_md5_free(&ctx);
        return false;
    }
    if (0 != mbedtls_md5_update_ret(&ctx, p_buf, buf_size))
    {
        mbedtls_md5_free(&ctx);
        return false;
    }
    if (0 != mbedtls_md5_finish_ret(&ctx, p_digest->buf))
    {
        mbedtls_md5_free(&ctx);
        return false;
    }
    mbedtls_md5_free(&ctx);
    return true;
}

wifiman_md5_digest_hex_str_t
wifiman_md5_calc_hex_str(const void *const p_buf, const size_t buf_size)
{
    wifiman_md5_digest_t digest = { 0 };
    if (!wifiman_md5_calc(p_buf, buf_size, &digest))
    {
        // return empty string
        const wifiman_md5_digest_hex_str_t digest_hex_str = { 0 };
        return digest_hex_str;
    }
    return wifiman_md5_hex_str(&digest);
}

bool
wifiman_md5_is_empty_digest_hex_str(const wifiman_md5_digest_hex_str_t *const p_digest_hex_str)
{
    if ('\0' == p_digest_hex_str->buf[0])
    {
        return true;
    }
    return false;
}
