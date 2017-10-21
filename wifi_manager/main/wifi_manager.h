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
