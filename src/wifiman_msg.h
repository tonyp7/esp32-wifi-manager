/**
 * @file wifiman_msg.h
 * @author TheSomeMan
 * @date 2020-11-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#ifndef WIFIMAN_MSG_H
#define WIFIMAN_MSG_H

#include <stdint.h>
#include <stdbool.h>
#include "sta_ip_unsafe.h"
#include "wifi_manager_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t wifiman_disconnection_reason_t;

bool
wifiman_msg_init(void);

void
wifiman_msg_deinit(void);

bool
wifiman_msg_recv(queue_message *p_msg);

connection_request_made_by_code_e
wifiman_conv_param_to_conn_req(const wifiman_msg_param_t *p_param);

sta_ip_address_t
wifiman_conv_param_to_ip_addr(const wifiman_msg_param_t *p_param);

wifiman_disconnection_reason_t
wifiman_conv_param_to_reason(const wifiman_msg_param_t *p_param);

bool
wifiman_msg_send_cmd_load_restore_sta(void);

bool
wifiman_msg_send_cmd_connect_sta(const connection_request_made_by_code_e conn_req_code);

bool
wifiman_msg_send_cmd_start_ap(void);

bool
wifiman_msg_send_cmd_disconnect_sta(void);

bool
wifiman_msg_send_cmd_start_wifi_scan(void);

bool
wifiman_msg_send_ev_scan_done(void);

bool
wifiman_msg_send_ev_disconnected(const wifiman_disconnection_reason_t reason);

bool
wifiman_msg_send_ev_got_ip(const sta_ip_address_t ip_addr);

#ifdef __cplusplus
}
#endif

#endif // WIFIMAN_MSG_H
