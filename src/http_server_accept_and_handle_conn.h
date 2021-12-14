/**
 * @file http_server_accept_and_handle_conn.h
 * @author TheSomeMan
 * @date 2021-11-20
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef HTTP_SERVER_ACCEPT_AND_HANDLE_CONN_H
#define HTTP_SERVER_ACCEPT_AND_HANDLE_CONN_H

#include "os_wrapper_types.h"
#include "lwip/api.h"

#ifdef __cplusplus
extern "C" {
#endif

os_delta_ticks_t
http_server_accept_and_handle_conn(struct netconn *const p_conn);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_ACCEPT_AND_HANDLE_CONN_H
