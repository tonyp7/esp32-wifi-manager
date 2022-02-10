/**
 * @file http_server_ecdh.h
 * @author TheSomeMan
 * @date 2022-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "http_server_ecdh.h"
#include <string.h>
#include "mbedtls/ecdh.h"
#include "mbedtls/base64.h"
#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"
#include "cJSON.h"
#include "esp_type_wrapper.h"
#include "os_malloc.h"

#define LOG_LOCAL_LEVEL LOG_LEVEL_INFO
#include "log.h"

#define HTTP_SERVER_ECDH_AES_KEY_SIZE   (32)
#define HTTP_SERVER_ECDH_AES_NUM_BITS   (HTTP_SERVER_ECDH_AES_KEY_SIZE * 8)
#define HTTP_SERVER_ECDH_AES_BLOCK_SIZE (16)

typedef struct http_server_ecdh_aes_key_t
{
    uint8_t buf[HTTP_SERVER_ECDH_AES_KEY_SIZE];
} http_server_ecdh_aes_key_t;

typedef struct http_server_ecdh_tls_handshake_message_t
{
    uint8_t buf[HTTP_SERVER_ECDH_TLS_HANDSHAKE_MESSAGE_OFFSET_PUB_KEY + HTTP_SERVER_ECDH_PUB_KEY_SIZE];
} http_server_ecdh_tls_handshake_message_t;

typedef struct http_server_ecdh_pub_key_with_len_t
{
    uint8_t buf[1 + HTTP_SERVER_ECDH_PUB_KEY_SIZE];
} http_server_ecdh_pub_key_with_len_t;

typedef struct http_server_ecdh_shared_secret_t
{
    uint8_t buf[HTTP_SERVER_ECDH_MPI_SIZE];
} http_server_ecdh_shared_secret_t;

typedef struct http_server_ecdh_aes_iv_t
{
    uint8_t buf[HTTP_SERVER_ECDH_AES_BLOCK_SIZE];
} http_server_ecdh_aes_iv_t;

typedef struct http_server_ecdh_array_buf_t
{
    uint8_t *p_buf;
    size_t   buf_size;
} http_server_ecdh_array_buf_t;

typedef struct http_server_ecdh_sha256_t
{
#define HTTP_SERVER_ECDH_SHA256_SIZE (32)
    uint8_t buf[HTTP_SERVER_ECDH_SHA256_SIZE];
} http_server_ecdh_sha256_t;

static const char *const TAG = "ECDH";

static int (*g_http_server_ecdh_f_rng)(void *, unsigned char *, size_t);
static void *g_http_server_ecdh_p_rng;

static mbedtls_ecdh_context       g_http_server_ecdh_ctx;
static http_server_ecdh_aes_key_t g_http_server_ecdh_aes_key;

bool
http_server_ecdh_init(int (*f_rng)(void *, unsigned char *, size_t), void *p_rng)
{
    if ((NULL == f_rng) || (NULL == p_rng))
    {
        LOG_ERR("Invalid param: f_rng=%p, p_rng=%p", f_rng, p_rng);
        return false;
    }

    g_http_server_ecdh_f_rng = f_rng;
    g_http_server_ecdh_p_rng = p_rng;
    mbedtls_ecdh_init(&g_http_server_ecdh_ctx);
    return true;
}

static bool
http_server_ecdh_b64_encode_pub_key(
    const http_server_ecdh_pub_key_bin_t *const p_pub_key_bin,
    http_server_ecdh_pub_key_b64_t *const       p_pub_key_b64)
{
    size_t olen = 0;
    if (0
        != mbedtls_base64_encode(
            (unsigned char *)p_pub_key_b64->buf,
            sizeof(p_pub_key_b64->buf),
            &olen,
            (const unsigned char *)p_pub_key_bin->buf,
            sizeof(p_pub_key_bin->buf)))
    {
        LOG_ERR("%s failed", "mbedtls_base64_encode");
        return false;
    }
    if ((sizeof(p_pub_key_b64->buf) - 1) != olen)
    {
        LOG_ERR(
            "mbedtls_base64_encode returned olen=%u, expected=%u",
            (printf_uint_t)olen,
            (printf_uint_t)(sizeof(p_pub_key_b64->buf) - 1));
        return false;
    }
    return true;
}

static bool
http_server_ecdh_b64_decode_pub_key_add_prefix_len(
    const char *const                          p_pub_key_b64,
    http_server_ecdh_pub_key_with_len_t *const p_pub_key_bin_with_len)
{
    size_t olen = 0;
    if (0
        != mbedtls_base64_decode(
            &p_pub_key_bin_with_len->buf[1],
            sizeof(p_pub_key_bin_with_len->buf) - 1,
            &olen,
            (const unsigned char *)p_pub_key_b64,
            strlen(p_pub_key_b64)))
    {
        LOG_ERR("%s failed", "mbedtls_base64_encode");
        return false;
    }
    if ((sizeof(p_pub_key_bin_with_len->buf) - 1) != olen)
    {
        LOG_ERR(
            "mbedtls_base64_encode returned olen=%u, expected=%u",
            (printf_uint_t)olen,
            (printf_uint_t)(sizeof(p_pub_key_bin_with_len->buf) - 1));
        return false;
    }
    p_pub_key_bin_with_len->buf[0] = (uint8_t)olen;
    return true;
}

static bool
http_server_ecdh_b64_decode_aes_iv(const char *const p_iv_b64, http_server_ecdh_aes_iv_t *const p_iv)
{
    size_t olen = 0;
    if (0
        != mbedtls_base64_decode(
            p_iv->buf,
            sizeof(p_iv->buf),
            &olen,
            (const unsigned char *)p_iv_b64,
            strlen(p_iv_b64)))
    {
        LOG_ERR("%s failed", "mbedtls_base64_encode");
        return false;
    }
    if (sizeof(p_iv->buf) != olen)
    {
        LOG_ERR(
            "mbedtls_base64_encode returned olen=%u, expected=%u",
            (printf_uint_t)olen,
            (printf_uint_t)sizeof(p_iv->buf));
        return false;
    }
    return true;
}

static bool
http_server_ecdh_b64_decode_hash(const char *const p_tag_b64, http_server_ecdh_sha256_t *const p_hash)
{
    size_t olen = 0;
    if (0
        != mbedtls_base64_decode(
            p_hash->buf,
            sizeof(p_hash->buf),
            &olen,
            (const unsigned char *)p_tag_b64,
            strlen(p_tag_b64)))
    {
        LOG_ERR("%s failed", "mbedtls_base64_encode");
        return false;
    }
    if (sizeof(p_hash->buf) != olen)
    {
        LOG_ERR(
            "mbedtls_base64_encode returned olen=%u, expected=%u",
            (printf_uint_t)olen,
            (printf_uint_t)sizeof(p_hash->buf));
        return false;
    }
    return true;
}

static bool
http_server_ecdh_b64_decode(const char *const p_b64_str, http_server_ecdh_array_buf_t *const p_arr_buf)
{
    size_t olen = 0;
    mbedtls_base64_decode(NULL, 0, &olen, (const unsigned char *)p_b64_str, strlen(p_b64_str));
    if (olen > HTTP_SERVER_ECDH_MAX_ENCRYPTED_LEN)
    {
        LOG_ERR(
            "The size of the encrypted content (%u) exceeds the allowable limit (%u)",
            (printf_uint_t)olen,
            HTTP_SERVER_ECDH_MAX_ENCRYPTED_LEN);
        return false;
    }

    p_arr_buf->p_buf = os_malloc(olen);
    if (NULL == p_arr_buf->p_buf)
    {
        LOG_ERR("Can't allocate %u bytes for encrypted buffer", (printf_uint_t)olen);
        return false;
    }
    p_arr_buf->buf_size = olen;

    olen = 0;
    if (0
        != mbedtls_base64_decode(
            (unsigned char *)p_arr_buf->p_buf,
            p_arr_buf->buf_size,
            &olen,
            (const unsigned char *)p_b64_str,
            strlen(p_b64_str)))
    {
        LOG_ERR("%s failed", "mbedtls_base64_decode");
        os_free(p_arr_buf->p_buf);
        p_arr_buf->p_buf = NULL;
        return false;
    }
    return true;
}

static void
http_server_ecdh_calc_sha256(
    const uint8_t *const             p_buf,
    const size_t                     buf_size,
    http_server_ecdh_sha256_t *const p_sha256)
{
    mbedtls_sha256_context sha256_ctx = { 0 };
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts_ret(&sha256_ctx, false);
    mbedtls_sha256_update_ret(&sha256_ctx, p_buf, buf_size);
    mbedtls_sha256_finish_ret(&sha256_ctx, p_sha256->buf);
    mbedtls_sha256_free(&sha256_ctx);
}

bool
http_server_ecdh_handshake(
    const http_server_ecdh_pub_key_b64_t *const p_pub_key_b64_cli,
    http_server_ecdh_pub_key_b64_t *const       p_pub_key_b64_srv)
{
    LOG_INFO("pub_key_b64_cli: %s", p_pub_key_b64_cli->buf);

    http_server_ecdh_pub_key_with_len_t pub_key_cli_with_len = { 0 };
    if (!http_server_ecdh_b64_decode_pub_key_add_prefix_len(p_pub_key_b64_cli->buf, &pub_key_cli_with_len))
    {
        LOG_ERR("Can't decode pub_key_b64_cli: %s", p_pub_key_b64_cli->buf);
        return false;
    }
    LOG_DUMP_DBG(pub_key_cli_with_len.buf, sizeof(pub_key_cli_with_len.buf), "PubKey_cli (with len)");

    mbedtls_ecdh_free(&g_http_server_ecdh_ctx);
    mbedtls_ecdh_init(&g_http_server_ecdh_ctx);

    if (0 != mbedtls_ecdh_setup(&g_http_server_ecdh_ctx, MBEDTLS_ECP_DP_SECP256R1))
    {
        LOG_ERR("%s failed", "mbedtls_ecdh_setup");
        return false;
    }

    size_t                                   handshake_message_len = 0;
    http_server_ecdh_tls_handshake_message_t handshake_message     = { 0 };
    if (0
        != mbedtls_ecdh_make_params(
            &g_http_server_ecdh_ctx,
            &handshake_message_len,
            handshake_message.buf,
            sizeof(handshake_message.buf),
            g_http_server_ecdh_f_rng,
            g_http_server_ecdh_p_rng))
    {
        LOG_ERR("%s failed", "mbedtls_ecdh_make_params");
        return false;
    }
    if (handshake_message_len != sizeof(handshake_message.buf))
    {
        LOG_ERR("mbedtls_ecdh_make_params return incorrect olen=%u", (printf_uint_t)handshake_message_len);
        return false;
    }
    http_server_ecdh_pub_key_bin_t pub_key_bin_srv;
    memcpy(
        pub_key_bin_srv.buf,
        &handshake_message.buf[HTTP_SERVER_ECDH_TLS_HANDSHAKE_MESSAGE_OFFSET_PUB_KEY],
        sizeof(pub_key_bin_srv.buf));
    LOG_DUMP_DBG(pub_key_bin_srv.buf, sizeof(pub_key_bin_srv.buf), "PubKey_srv");

    if (!http_server_ecdh_b64_encode_pub_key(&pub_key_bin_srv, p_pub_key_b64_srv))
    {
        LOG_ERR("%s failed", "http_server_ecdh_b64_encode_pub_key");
        return false;
    }

    LOG_INFO("pub_key_b64_srv: %s", p_pub_key_b64_srv->buf);

    if (0
        != mbedtls_ecdh_read_public(
            &g_http_server_ecdh_ctx,
            pub_key_cli_with_len.buf,
            sizeof(pub_key_cli_with_len.buf)))
    {
        LOG_ERR("%s failed", "mbedtls_ecdh_read_public");
        return false;
    }

    size_t                           len_shared_secret = 0;
    http_server_ecdh_shared_secret_t shared_secret;
    if (0
        != mbedtls_ecdh_calc_secret(
            &g_http_server_ecdh_ctx,
            &len_shared_secret,
            shared_secret.buf,
            sizeof(shared_secret.buf),
            g_http_server_ecdh_f_rng,
            g_http_server_ecdh_p_rng))
    {
        LOG_ERR("%s failed", "mbedtls_ecdh_calc_secret");
        return false;
    }
    if (sizeof(shared_secret.buf) != len_shared_secret)
    {
        LOG_ERR("%s failed", "mbedtls_ecdh_calc_secret");
        return false;
    }
    LOG_DUMP_DBG(shared_secret.buf, sizeof(shared_secret.buf), "Shared secret");

    http_server_ecdh_sha256_t sha256 = { 0 };
    http_server_ecdh_calc_sha256(shared_secret.buf, sizeof(shared_secret.buf), &sha256);
    memcpy(g_http_server_ecdh_aes_key.buf, sha256.buf, sizeof(g_http_server_ecdh_aes_key.buf));
    LOG_DUMP_DBG(g_http_server_ecdh_aes_key.buf, sizeof(g_http_server_ecdh_aes_key.buf), "AES key");

    return true;
}

static bool
http_server_ecdh_aes_decrypt(
    const http_server_ecdh_array_buf_t *const p_encrypted_buf,
    http_server_ecdh_aes_iv_t *               p_aes_iv,
    http_server_ecdh_array_buf_t *const       p_decrypted_buf)
{
    mbedtls_aes_context aes_ctx = { 0 };
    mbedtls_aes_init(&aes_ctx);

    mbedtls_aes_setkey_dec(&aes_ctx, g_http_server_ecdh_aes_key.buf, HTTP_SERVER_ECDH_AES_NUM_BITS);
    mbedtls_aes_crypt_cbc(
        &aes_ctx,
        MBEDTLS_AES_DECRYPT,
        p_encrypted_buf->buf_size,
        p_aes_iv->buf,
        p_encrypted_buf->p_buf,
        p_decrypted_buf->p_buf);
    const uint8_t pad_len = p_decrypted_buf->p_buf[p_decrypted_buf->buf_size - 1];
    LOG_DBG("pad_len=%d", pad_len);
    if (pad_len > HTTP_SERVER_ECDH_AES_BLOCK_SIZE)
    {
        return false;
    }
    if (pad_len > p_decrypted_buf->buf_size)
    {
        return false;
    }
    p_decrypted_buf->buf_size -= pad_len;
    mbedtls_aes_free(&aes_ctx);
    return true;
}

bool
http_server_ecdh_decrypt(const http_server_ecdh_encrypted_req_t *const p_enc_req, str_buf_t *p_str_buf)
{
    LOG_DBG("IV: %s", p_enc_req->p_iv);
    LOG_DBG("Hash: %s", p_enc_req->p_hash);
    LOG_DBG("Encrypted: %s", p_enc_req->p_encrypted);

    http_server_ecdh_aes_iv_t aes_iv = { 0 };
    if (!http_server_ecdh_b64_decode_aes_iv(p_enc_req->p_iv, &aes_iv))
    {
        LOG_ERR("Failed to decode IV");
        return false;
    }
    LOG_DUMP_DBG(aes_iv.buf, sizeof(aes_iv.buf), "IV");

    http_server_ecdh_sha256_t hash_expected = { 0 };
    if (!http_server_ecdh_b64_decode_hash(p_enc_req->p_hash, &hash_expected))
    {
        LOG_ERR("Failed to decode Hash");
        return false;
    }
    LOG_DUMP_DBG(hash_expected.buf, sizeof(hash_expected.buf), "Hash");

    http_server_ecdh_array_buf_t encrypted_arr_buf = { 0 };
    if (!http_server_ecdh_b64_decode(p_enc_req->p_encrypted, &encrypted_arr_buf))
    {
        LOG_ERR("Failed to decode encrypted content");
        return false;
    }
    LOG_DUMP_DBG(encrypted_arr_buf.p_buf, encrypted_arr_buf.buf_size, "Encrypted content");

    const size_t buf_size = encrypted_arr_buf.buf_size;

    if ((buf_size % HTTP_SERVER_ECDH_AES_BLOCK_SIZE) != 0)
    {
        LOG_ERR("Encrypted buf length is not multiple of 16");
        os_free(encrypted_arr_buf.p_buf);
        return false;
    }

    http_server_ecdh_array_buf_t decrypted_arr_buf = {
        .p_buf    = os_malloc(buf_size),
        .buf_size = buf_size,
    };
    if (NULL == decrypted_arr_buf.p_buf)
    {
        LOG_ERR("Can't allocate %u bytes for decrypted buf", (printf_uint_t)(buf_size + 1));
        os_free(encrypted_arr_buf.p_buf);
        return false;
    }
    if (!http_server_ecdh_aes_decrypt(&encrypted_arr_buf, &aes_iv, &decrypted_arr_buf))
    {
        LOG_ERR("Failed to decrypt");
        os_free(encrypted_arr_buf.p_buf);
        os_free(decrypted_arr_buf.p_buf);
        return false;
    }
    LOG_DUMP_DBG(decrypted_arr_buf.p_buf, decrypted_arr_buf.buf_size, "Decrypted:");

    os_free(encrypted_arr_buf.p_buf);

    http_server_ecdh_sha256_t hash_sha256 = { 0 };
    http_server_ecdh_calc_sha256(decrypted_arr_buf.p_buf, decrypted_arr_buf.buf_size, &hash_sha256);
    LOG_DUMP_DBG(hash_sha256.buf, sizeof(hash_sha256.buf), "HASH(SHA256):");

    if (0 != memcmp(hash_sha256.buf, hash_expected.buf, sizeof(hash_sha256.buf)))
    {
        LOG_ERR("Hashes does not match");
        os_free(decrypted_arr_buf.p_buf);
        return false;
    }

    *p_str_buf = str_buf_printf_with_alloc(
        "%.*s",
        (printf_int_t)(decrypted_arr_buf.buf_size + 1),
        decrypted_arr_buf.p_buf);
    os_free(decrypted_arr_buf.p_buf);
    if (NULL == p_str_buf->buf)
    {
        LOG_ERR("Failed to create decrypted_str_buf");
        return false;
    }

    LOG_DBG("Decrypted json: %s", p_str_buf->buf);

    return true;
}
