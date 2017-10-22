/**
 * \file wifi_manager.h
 * \author Tony Pottier
 * \brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis
 *
 * Contains the freeRTOS task and all necessary support
 *
 * \see https://idyl.io
 * \see https://github.com/tonyp7/esp32-wifi-manager
 */

#ifndef MAIN_WIFI_MANAGER_H_
#define MAIN_WIFI_MANAGER_H_

#define MAX_AP_NUM 30


// set AP CONFIG values
#define AP_AUTHMODE WIFI_AUTH_WPA2_PSK
#define AP_SSID_HIDDEN 0
#define AP_SSID "esp32"
#define AP_PASSWORD "esp32pwd"
#define AP_CHANNEL 8
#define AP_MAX_CONNECTIONS 4
#define AP_BEACON_INTERVAL 100



/*!
* \brief define the maximum length in bytes of a JSON representation of an access point.
*
*  maximum ap string length with full 32 char ssid: 75 + \\n + \0 = 77\n
*  example: {"ssid":"abcdefghijklmnopqrstuvwxyz012345","chan":12,"rssi":-100,"auth":4},\n
*  BUT: we need to escape JSON. Imagine a ssid full of \" ? so it's 32 more bytes hence 77 + 32 = 99.\n
*  this is an edge case but I don't think we should crash in a catastrophic manner just because
*  someone decided to have a funny wifi name.
*/
#define JSON_ONE_APP_SIZE 99


void wifi_scan_init();
void wifi_scan_destroy();
void wifi_manager( void * pvParameters );
void wifi_scan_generate_json();
void wifi_manager_init();
char* wifi_scan_get_json();


wifi_sta_config_t* get_wifi_sta_config();


esp_err_t wifi_manager_event_handler(void *ctx, system_event_t *event);


uint8_t wifi_scan_lock_ap_list();
void wifi_scan_unlock_ap_list();

#endif /* MAIN_WIFI_MANAGER_H_ */
