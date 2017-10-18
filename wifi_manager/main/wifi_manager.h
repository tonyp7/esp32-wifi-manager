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

void wifi_scan_init();
void wifi_scan_destroy();
void wifi_manager( void * pvParameters );
void wifi_scan_generate_json();
char* wifi_scan_get_json();

wifi_sta_config_t* get_wifi_sta_config();

uint8_t wifi_scan_lock_ap_list();
void wifi_scan_unlock_ap_list();

#endif /* MAIN_WIFI_MANAGER_H_ */
