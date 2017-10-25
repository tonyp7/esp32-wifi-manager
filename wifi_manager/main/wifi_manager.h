/**
 * @file wifi_manager.h
 * @author Tony Pottier
 * @brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis
 *
 * Contains the freeRTOS task and all necessary support.
 *
 * @see https://idyl.io
 * @see https://github.com/tonyp7/esp32-wifi-manager
 */

#ifndef MAIN_WIFI_MANAGER_H_
#define MAIN_WIFI_MANAGER_H_


/**
 * @brief Defines the maximum size of a SSID name. 32 is IEEE standard.
 * @warning limit is also hard coded in wifi_config_t. Never extend this value.
 */
#define MAX_SSID_SIZE		32

/**
 * @brief Defines the maximum size of a WPA2 passkey. 64 is IEEE standard.
 * @warning limit is also hard coded in wifi_config_t. Never extend this value.
 */
#define MAX_PASSWORD_SIZE	64


/**
 * @brief Defines the maximum number of access points that can be detected.
 *
 * To save memory and avoid nasty out of memory errors,
 * we can limit the number of APs detected in a wifi scan.
 */
#define MAX_AP_NUM 			30


/** @brief Defines the auth mode as an access point
 *  Value must be of type wifi_auth_mode_t
 *  @see esp_wifi_types.h
 */
#define AP_AUTHMODE 		WIFI_AUTH_WPA2_PSK

/** @brief Defines visibility of the access point. 0: visible AP. 1: hidden */
#define AP_SSID_HIDDEN 		0

/** @brief Defines access point's name. */
#define AP_SSID 			"esp32"

/** @brief Defines access point's password.
 *	@warning In the case of an open access point, the password must be a null string "" or "\0" if you want to be verbose but waste one byte.
 */
#define AP_PASSWORD 		"esp32pwd"

/** @brief Defines access point's channel. */
#define AP_CHANNEL 			8

/** @brief Defines access point's maximum number of clients. */
#define AP_MAX_CONNECTIONS 	4

/** @brief Defines access point's beacon interval. 100ms is the recommended default. */
#define AP_BEACON_INTERVAL 	100



/**
 * @brief Defines the maximum length in bytes of a JSON representation of an access point.
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


uint8_t wifi_manager_fetch_wifi_sta_config();

wifi_config_t* wifi_manager_get_wifi_sta_config();


esp_err_t wifi_manager_event_handler(void *ctx, system_event_t *event);


uint8_t wifi_scan_lock_ap_list();
void wifi_scan_unlock_ap_list();

#endif /* MAIN_WIFI_MANAGER_H_ */
