/**
 * @file http_server_ecdh.h
 * @author TheSomeMan
 * @date 2022-02-03
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef RUUVI_GATEWAY_ESP_HTTP_SERVER_ECDH_H
#define RUUVI_GATEWAY_ESP_HTTP_SERVER_ECDH_H

#include <stdbool.h>
#include <stddef.h>
#include <str_buf.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_SERVER_ECDH_MAX_ENCRYPTED_LEN (20 * 1024)

#define HTTP_SERVER_ECDH_MPI_SIZE                             (32)
#define HTTP_SERVER_ECDH_TLS_HANDSHAKE_MESSAGE_OFFSET_PUB_KEY (3 + 1 /* grp, len */)
#define HTTP_SERVER_ECDH_PUB_KEY_OFFSET                       (1)

#define HTTP_SERVER_ECDH_PUB_KEY_SIZE (HTTP_SERVER_ECDH_PUB_KEY_OFFSET + 2 * HTTP_SERVER_ECDH_MPI_SIZE)

#define HTTP_SERVER_ECDH_CALC_B64_SIZE(bin_size_) ((((bin_size_) / 3) + (((bin_size_) % 3) != 0)) * 4 + 1)

typedef struct http_server_ecdh_pub_key_bin_t
{
    uint8_t buf[1 + 2 * HTTP_SERVER_ECDH_MPI_SIZE];
} http_server_ecdh_pub_key_bin_t;

typedef struct http_server_ecdh_pub_key_b64_t
{
    char buf[HTTP_SERVER_ECDH_CALC_B64_SIZE(HTTP_SERVER_ECDH_PUB_KEY_SIZE)];
} http_server_ecdh_pub_key_b64_t;

typedef struct http_server_ecdh_encrypted_req_t
{
    const char *p_encrypted;
    const char *p_iv;
    const char *p_hash;
} http_server_ecdh_encrypted_req_t;

bool
http_server_ecdh_init(int (*f_rng)(void *, unsigned char *, size_t), void *p_rng);

bool
http_server_ecdh_handshake(
    const http_server_ecdh_pub_key_b64_t *const p_pub_key_b64_cli,
    http_server_ecdh_pub_key_b64_t *const       p_pub_key_b64_srv);

bool
http_server_ecdh_decrypt(const http_server_ecdh_encrypted_req_t *const p_enc_req, str_buf_t *const p_str_buf);

#ifdef __cplusplus
}
#endif

#endif // RUUVI_GATEWAY_ESP_HTTP_SERVER_ECDH_H
