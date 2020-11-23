/**
 * @file wifiman_msg.c
 * @author TheSomeMan
 * @date 2020-11-07
 * @copyright Ruuvi Innovations Ltd, license BSD-3-Clause.
 */

#include "wifiman_msg.h"
#include "wifi_manager_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "log.h"

static QueueHandle_t gh_wifiman_msg_queue;

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifi_manager";

bool
wifiman_msg_init(void)
{
    const uint32_t queue_length = 3U;

    gh_wifiman_msg_queue = xQueueCreate(queue_length, sizeof(queue_message));
    if (NULL == gh_wifiman_msg_queue)
    {
        LOG_ERR("%s failed", "xQueueCreate");
        return false;
    }
    return true;
}

void
wifiman_msg_deinit(void)
{
    vQueueDelete(gh_wifiman_msg_queue);
    gh_wifiman_msg_queue = NULL;
}

bool
wifiman_msg_recv(queue_message *p_msg)
{
    const BaseType_t xStatus = xQueueReceive(gh_wifiman_msg_queue, p_msg, portMAX_DELAY);
    if (pdPASS != xStatus)
    {
        LOG_ERR("%s failed", "xQueueReceive");
        return false;
    }
    return true;
}

connection_request_made_by_code_e
wifiman_conv_param_to_conn_req(const wifiman_msg_param_t *p_param)
{
    const connection_request_made_by_code_e conn_req = (connection_request_made_by_code_e)p_param->val;
    return conn_req;
}

sta_ip_address_t
wifiman_conv_param_to_ip_addr(const wifiman_msg_param_t *p_param)
{
    const sta_ip_address_t ip_addr = p_param->val;
    return ip_addr;
}

wifiman_disconnection_reason_t
wifiman_conv_param_to_reason(const wifiman_msg_param_t *p_param)
{
    const wifiman_disconnection_reason_t reason = (wifiman_disconnection_reason_t)p_param->val;
    return reason;
}

static bool
wifiman_msg_send(const message_code_e code, const wifiman_msg_param_t msg_param)
{
    const queue_message msg = {
        .code      = code,
        .msg_param = msg_param,
    };
    if (pdTRUE != xQueueSend(gh_wifiman_msg_queue, &msg, portMAX_DELAY))
    {
        LOG_ERR("%s failed", "xQueueSend");
        return false;
    }
    return true;
}

bool
wifiman_msg_send_cmd_load_restore_sta(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_LOAD_AND_RESTORE_STA, msg_param);
}

bool
wifiman_msg_send_cmd_start_ap(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_START_AP, msg_param);
}

bool
wifiman_msg_send_cmd_connect_sta(const connection_request_made_by_code_e conn_req_code)
{
    const wifiman_msg_param_t msg_param = {
        .val = conn_req_code,
    };
    return wifiman_msg_send(ORDER_CONNECT_STA, msg_param);
}

bool
wifiman_msg_send_cmd_disconnect_sta(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_DISCONNECT_STA, msg_param);
}

bool
wifiman_msg_send_cmd_start_wifi_scan(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(ORDER_START_WIFI_SCAN, msg_param);
}

bool
wifiman_msg_send_ev_scan_done(void)
{
    const wifiman_msg_param_t msg_param = {
        .ptr = NULL,
    };
    return wifiman_msg_send(EVENT_SCAN_DONE, msg_param);
}

bool
wifiman_msg_send_ev_got_ip(const sta_ip_address_t ip_addr)
{
    const wifiman_msg_param_t msg_param = {
        .val = ip_addr,
    };
    return wifiman_msg_send(EVENT_STA_GOT_IP, msg_param);
}

bool
wifiman_msg_send_ev_disconnected(const wifiman_disconnection_reason_t reason)
{
    const wifiman_msg_param_t msg_param = {
        .val = reason,
    };
    return wifiman_msg_send(EVENT_STA_DISCONNECTED, msg_param);
}
