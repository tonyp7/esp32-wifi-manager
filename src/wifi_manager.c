/*
Copyright (c) 2017-2020 Tony Pottier

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

@file wifi_manager.c
@author Tony Pottier
@brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis

Contains the freeRTOS task and all necessary support

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_system.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <freertos/timers.h>
#include <http_app.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/ip4_addr.h"


#include "json.h"
#include "dns_server.h"
#include "nvs_sync.h"
#include "wifi_manager.h"



/* objects used to manipulate the main queue of events */
QueueHandle_t wifi_manager_queue;

/* @brief software timer to wait between each connection retry.
 * There is no point hogging a hardware timer for a functionality like this which only needs to be 'accurate enough' */
TimerHandle_t wifi_manager_retry_timer = NULL;

/* @brief software timer that will trigger shutdown of the AP after a succesful STA connection
 * There is no point hogging a hardware timer for a functionality like this which only needs to be 'accurate enough' */
TimerHandle_t wifi_manager_shutdown_ap_timer = NULL;

SemaphoreHandle_t wifi_manager_json_mutex = NULL;
SemaphoreHandle_t wifi_manager_sta_ip_mutex = NULL;
char *wifi_manager_sta_ip = NULL;
uint16_t ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records;
char *accessp_json = NULL;
char *ip_info_json = NULL;
wifi_config_t* wifi_manager_config_sta = NULL;

/* @brief Array of callback function pointers */
void (**cb_ptr_arr)(void*) = NULL;

/* @brief tag used for ESP serial console messages */
static const char TAG[] = "wifi_manager";

/* @brief task handle for the main wifi_manager task */
static TaskHandle_t task_wifi_manager = NULL;

/* @brief netif object for the STATION */
static esp_netif_t* esp_netif_sta = NULL;

/* @brief netif object for the ACCESS POINT */
static esp_netif_t* esp_netif_ap = NULL;

/**
 * The actual WiFi settings in use
 */
struct wifi_settings_t wifi_settings = {
	.ap_ssid = DEFAULT_AP_SSID,
	.ap_pwd = DEFAULT_AP_PASSWORD,
	.ap_channel = DEFAULT_AP_CHANNEL,
	.ap_ssid_hidden = DEFAULT_AP_SSID_HIDDEN,
	.ap_bandwidth = DEFAULT_AP_BANDWIDTH,
	.sta_only = DEFAULT_STA_ONLY,
	.sta_power_save = DEFAULT_STA_POWER_SAVE,
	.sta_static_ip = 0,
};

const char wifi_manager_nvs_namespace[] = "espwifimgr";

static EventGroupHandle_t wifi_manager_event_group;

/* @brief indicate that the ESP32 is currently connected. */
const int WIFI_MANAGER_WIFI_CONNECTED_BIT = BIT0;

const int WIFI_MANAGER_AP_STA_CONNECTED_BIT = BIT1;

/* @brief Set automatically once the SoftAP is started */
const int WIFI_MANAGER_AP_STARTED_BIT = BIT2;

/* @brief When set, means a client requested to connect to an access point.*/
const int WIFI_MANAGER_REQUEST_STA_CONNECT_BIT = BIT3;

/* @brief This bit is set automatically as soon as a connection was lost */
const int WIFI_MANAGER_STA_DISCONNECT_BIT = BIT4;

/* @brief When set, means the wifi manager attempts to restore a previously saved connection at startup. */
const int WIFI_MANAGER_REQUEST_RESTORE_STA_BIT = BIT5;

/* @brief When set, means a client requested to disconnect from currently connected AP. */
const int WIFI_MANAGER_REQUEST_WIFI_DISCONNECT_BIT = BIT6;

/* @brief When set, means a scan is in progress */
const int WIFI_MANAGER_SCAN_BIT = BIT7;

/* @brief When set, means user requested for a disconnect */
const int WIFI_MANAGER_REQUEST_DISCONNECT_BIT = BIT8;



void wifi_manager_timer_retry_cb( TimerHandle_t xTimer ){

	ESP_LOGI(TAG, "Retry Timer Tick! Sending ORDER_CONNECT_STA with reason CONNECTION_REQUEST_AUTO_RECONNECT");

	/* stop the timer */
	xTimerStop( xTimer, (TickType_t) 0 );

	/* Attempt to reconnect */
	wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_AUTO_RECONNECT);

}

void wifi_manager_timer_shutdown_ap_cb( TimerHandle_t xTimer){

	/* stop the timer */
	xTimerStop( xTimer, (TickType_t) 0 );

	/* Attempt to shutdown AP */
	wifi_manager_send_message(WM_ORDER_STOP_AP, NULL);
}

void wifi_manager_scan_async(){
	wifi_manager_send_message(WM_ORDER_START_WIFI_SCAN, NULL);
}

void wifi_manager_disconnect_async(){
	wifi_manager_send_message(WM_ORDER_DISCONNECT_STA, NULL);
}


void wifi_manager_start(){

	/* disable the default wifi logging */
	esp_log_level_set("wifi", ESP_LOG_NONE);

	/* initialize flash memory */
	nvs_flash_init();
	ESP_ERROR_CHECK(nvs_sync_create()); /* semaphore for thread synchronization on NVS memory */

	/* memory allocation */
	wifi_manager_queue = xQueueCreate( 3, sizeof( queue_message) );
	wifi_manager_json_mutex = xSemaphoreCreateMutex();
	accessp_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * MAX_AP_NUM);
	accessp_json = (char*)malloc(MAX_AP_NUM * JSON_ONE_APP_SIZE + 4); /* 4 bytes for json encapsulation of "[\n" and "]\0" */
	wifi_manager_clear_access_points_json();
	ip_info_json = (char*)malloc(sizeof(char) * JSON_IP_INFO_SIZE);
	wifi_manager_clear_ip_info_json();
	wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
	memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
	memset(&wifi_settings.sta_static_ip_config, 0x00, sizeof(esp_netif_ip_info_t));
	cb_ptr_arr = malloc(sizeof(void (*)(void*)) * WM_MESSAGE_CODE_COUNT);
	for(int i=0; i<WM_MESSAGE_CODE_COUNT; i++){
		cb_ptr_arr[i] = NULL;
	}
	wifi_manager_sta_ip_mutex = xSemaphoreCreateMutex();
	wifi_manager_sta_ip = (char*)malloc(sizeof(char) * IP4ADDR_STRLEN_MAX);
	wifi_manager_safe_update_sta_ip_string((uint32_t)0);
	wifi_manager_event_group = xEventGroupCreate();

	/* create timer for to keep track of retries */
	wifi_manager_retry_timer = xTimerCreate( NULL, pdMS_TO_TICKS(WIFI_MANAGER_RETRY_TIMER), pdFALSE, ( void * ) 0, wifi_manager_timer_retry_cb);

	/* create timer for to keep track of AP shutdown */
	wifi_manager_shutdown_ap_timer = xTimerCreate( NULL, pdMS_TO_TICKS(WIFI_MANAGER_SHUTDOWN_AP_TIMER), pdFALSE, ( void * ) 0, wifi_manager_timer_shutdown_ap_cb);

	/* start wifi manager task */
	xTaskCreate(&wifi_manager, "wifi_manager", 4096, NULL, WIFI_MANAGER_TASK_PRIORITY, &task_wifi_manager);
}

esp_err_t wifi_manager_save_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	size_t sz;

	/* variables used to check if write is really needed */
	wifi_config_t tmp_conf;
	struct wifi_settings_t tmp_settings;
	memset(&tmp_conf, 0x00, sizeof(tmp_conf));
	memset(&tmp_settings, 0x00, sizeof(tmp_settings));
	bool change = false;

	ESP_LOGI(TAG, "About to save config to flash!!");

	if(wifi_manager_config_sta && nvs_sync_lock( portMAX_DELAY )){

		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READWRITE, &handle);
		if (esp_err != ESP_OK){
			nvs_sync_unlock();
			return esp_err;
		}

		sz = sizeof(tmp_conf.sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", tmp_conf.sta.ssid, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp( (char*)tmp_conf.sta.ssid, (char*)wifi_manager_config_sta->sta.ssid) != 0){
			/* different ssid or ssid does not exist in flash: save new ssid */
			esp_err = nvs_set_blob(handle, "ssid", wifi_manager_config_sta->sta.ssid, 32);
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: ssid:%s",wifi_manager_config_sta->sta.ssid);

		}

		sz = sizeof(tmp_conf.sta.password);
		esp_err = nvs_get_blob(handle, "password", tmp_conf.sta.password, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) && strcmp( (char*)tmp_conf.sta.password, (char*)wifi_manager_config_sta->sta.password) != 0){
			/* different password or password does not exist in flash: save new password */
			esp_err = nvs_set_blob(handle, "password", wifi_manager_config_sta->sta.password, 64);
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;
			ESP_LOGI(TAG, "wifi_manager_wrote wifi_sta_config: password:%s",wifi_manager_config_sta->sta.password);
		}

		sz = sizeof(tmp_settings);
		esp_err = nvs_get_blob(handle, "settings", &tmp_settings, &sz);
		if( (esp_err == ESP_OK  || esp_err == ESP_ERR_NVS_NOT_FOUND) &&
				(
				strcmp( (char*)tmp_settings.ap_ssid, (char*)wifi_settings.ap_ssid) != 0 ||
				strcmp( (char*)tmp_settings.ap_pwd, (char*)wifi_settings.ap_pwd) != 0 ||
				tmp_settings.ap_ssid_hidden != wifi_settings.ap_ssid_hidden ||
				tmp_settings.ap_bandwidth != wifi_settings.ap_bandwidth ||
				tmp_settings.sta_only != wifi_settings.sta_only ||
				tmp_settings.sta_power_save != wifi_settings.sta_power_save ||
				tmp_settings.ap_channel != wifi_settings.ap_channel
				)
		){
			esp_err = nvs_set_blob(handle, "settings", &wifi_settings, sizeof(wifi_settings));
			if (esp_err != ESP_OK){
				nvs_sync_unlock();
				return esp_err;
			}
			change = true;

			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_ssid: %s",wifi_settings.ap_ssid);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_pwd: %s",wifi_settings.ap_pwd);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_channel: %i",wifi_settings.ap_channel);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_hidden (1 = yes): %i",wifi_settings.ap_ssid_hidden);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz): %i",wifi_settings.ap_bandwidth);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_only (0 = APSTA, 1 = STA when connected): %i",wifi_settings.sta_only);
			ESP_LOGD(TAG, "wifi_manager_wrote wifi_settings: sta_power_save (1 = yes): %i",wifi_settings.sta_power_save);
		}

		if(change){
			esp_err = nvs_commit(handle);
		}
		else{
			ESP_LOGI(TAG, "Wifi config was not saved to flash because no change has been detected.");
		}

		if (esp_err != ESP_OK) return esp_err;

		nvs_close(handle);
		nvs_sync_unlock();

	}
	else{
		ESP_LOGE(TAG, "wifi_manager_save_sta_config failed to acquire nvs_sync mutex");
	}

	return ESP_OK;
}

bool wifi_manager_fetch_wifi_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	if(nvs_sync_lock( portMAX_DELAY )){

		esp_err = nvs_open(wifi_manager_nvs_namespace, NVS_READONLY, &handle);

		if(esp_err != ESP_OK){
			nvs_sync_unlock();
			return false;
		}

		if(wifi_manager_config_sta == NULL){
			wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
		}
		memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

		/* allocate buffer */
		size_t sz = sizeof(wifi_settings);
		uint8_t *buff = (uint8_t*)malloc(sizeof(uint8_t) * sz);
		memset(buff, 0x00, sizeof(sz));

		/* ssid */
		sz = sizeof(wifi_manager_config_sta->sta.ssid);
		esp_err = nvs_get_blob(handle, "ssid", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.ssid, buff, sz);

		/* password */
		sz = sizeof(wifi_manager_config_sta->sta.password);
		esp_err = nvs_get_blob(handle, "password", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(wifi_manager_config_sta->sta.password, buff, sz);

		/* settings */
		sz = sizeof(wifi_settings);
		esp_err = nvs_get_blob(handle, "settings", buff, &sz);
		if(esp_err != ESP_OK){
			free(buff);
			nvs_sync_unlock();
			return false;
		}
		memcpy(&wifi_settings, buff, sz);

		free(buff);
		nvs_close(handle);
		nvs_sync_unlock();


		ESP_LOGI(TAG, "wifi_manager_fetch_wifi_sta_config: ssid:%s password:%s",wifi_manager_config_sta->sta.ssid,wifi_manager_config_sta->sta.password);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_ssid:%s",wifi_settings.ap_ssid);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_pwd:%s",wifi_settings.ap_pwd);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_channel:%i",wifi_settings.ap_channel);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_hidden (1 = yes):%i",wifi_settings.ap_ssid_hidden);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: SoftAP_bandwidth (1 = 20MHz, 2 = 40MHz)%i",wifi_settings.ap_bandwidth);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_only (0 = APSTA, 1 = STA when connected):%i",wifi_settings.sta_only);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_power_save (1 = yes):%i",wifi_settings.sta_power_save);
		ESP_LOGD(TAG, "wifi_manager_fetch_wifi_settings: sta_static_ip (0 = dhcp client, 1 = static ip):%i",wifi_settings.sta_static_ip);

		return wifi_manager_config_sta->sta.ssid[0] != '\0';


	}
	else{
		return false;
	}

}


void wifi_manager_clear_ip_info_json(){
	strcpy(ip_info_json, "{}\n");
}


void wifi_manager_generate_ip_info_json(update_reason_code_t update_reason_code){

	wifi_config_t *config = wifi_manager_get_wifi_sta_config();
	if(config){

		const char *ip_info_json_format = ",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\",\"urc\":%d}\n";

		memset(ip_info_json, 0x00, JSON_IP_INFO_SIZE);

		/* to avoid declaring a new buffer we copy the data directly into the buffer at its correct address */
		strcpy(ip_info_json, "{\"ssid\":");
		json_print_string(config->sta.ssid,  (unsigned char*)(ip_info_json+strlen(ip_info_json)) );

		size_t ip_info_json_len = strlen(ip_info_json);
		size_t remaining = JSON_IP_INFO_SIZE - ip_info_json_len;
		if(update_reason_code == UPDATE_CONNECTION_OK){
			/* rest of the information is copied after the ssid */
			esp_netif_ip_info_t ip_info;
			ESP_ERROR_CHECK(esp_netif_get_ip_info(esp_netif_sta, &ip_info));

			char ip[IP4ADDR_STRLEN_MAX]; /* note: IP4ADDR_STRLEN_MAX is defined in lwip */
			char gw[IP4ADDR_STRLEN_MAX];
			char netmask[IP4ADDR_STRLEN_MAX];

			esp_ip4addr_ntoa(&ip_info.ip, ip, IP4ADDR_STRLEN_MAX);
			esp_ip4addr_ntoa(&ip_info.gw, gw, IP4ADDR_STRLEN_MAX);
			esp_ip4addr_ntoa(&ip_info.netmask, netmask, IP4ADDR_STRLEN_MAX);


			snprintf( (ip_info_json + ip_info_json_len), remaining, ip_info_json_format,
					ip,
					netmask,
					gw,
					(int)update_reason_code);
		}
		else{
			/* notify in the json output the reason code why this was updated without a connection */
			snprintf( (ip_info_json + ip_info_json_len), remaining, ip_info_json_format,
								"0",
								"0",
								"0",
								(int)update_reason_code);
		}
	}
	else{
		wifi_manager_clear_ip_info_json();
	}


}


void wifi_manager_clear_access_points_json(){
	strcpy(accessp_json, "[]\n");
}
void wifi_manager_generate_acess_points_json(){

	strcpy(accessp_json, "[");


	const char oneap_str[] = ",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%c\n";

	/* stack buffer to hold on to one AP until it's copied over to accessp_json */
	char one_ap[JSON_ONE_APP_SIZE];
	for(int i=0; i<ap_num;i++){

		wifi_ap_record_t ap = accessp_records[i];

		/* ssid needs to be json escaped. To save on heap memory it's directly printed at the correct address */
		strcat(accessp_json, "{\"ssid\":");
		json_print_string( (unsigned char*)ap.ssid,  (unsigned char*)(accessp_json+strlen(accessp_json)) );

		/* print the rest of the json for this access point: no more string to escape */
		snprintf(one_ap, (size_t)JSON_ONE_APP_SIZE, oneap_str,
				ap.primary,
				ap.rssi,
				ap.authmode,
				i==ap_num-1?']':',');

		/* add it to the list */
		strcat(accessp_json, one_ap);
	}

}



bool wifi_manager_lock_sta_ip_string(TickType_t xTicksToWait){
	if(wifi_manager_sta_ip_mutex){
		if( xSemaphoreTake( wifi_manager_sta_ip_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
void wifi_manager_unlock_sta_ip_string(){
	xSemaphoreGive( wifi_manager_sta_ip_mutex );
}

void wifi_manager_safe_update_sta_ip_string(uint32_t ip){

	if(wifi_manager_lock_sta_ip_string(portMAX_DELAY)){

		esp_ip4_addr_t ip4;
		ip4.addr = ip;

		char str_ip[IP4ADDR_STRLEN_MAX];
		esp_ip4addr_ntoa(&ip4, str_ip, IP4ADDR_STRLEN_MAX);

		strcpy(wifi_manager_sta_ip, str_ip);

		ESP_LOGI(TAG, "Set STA IP String to: %s", wifi_manager_sta_ip);

		wifi_manager_unlock_sta_ip_string();
	}
}

char* wifi_manager_get_sta_ip_string(){
	return wifi_manager_sta_ip;
}


bool wifi_manager_lock_json_buffer(TickType_t xTicksToWait){
	if(wifi_manager_json_mutex){
		if( xSemaphoreTake( wifi_manager_json_mutex, xTicksToWait ) == pdTRUE ) {
			return true;
		}
		else{
			return false;
		}
	}
	else{
		return false;
	}

}
void wifi_manager_unlock_json_buffer(){
	xSemaphoreGive( wifi_manager_json_mutex );
}

char* wifi_manager_get_ap_list_json(){
	return accessp_json;
}


/**
 * @brief Standard wifi event handler
 */
static void wifi_manager_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){


	if (event_base == WIFI_EVENT){

		switch(event_id){

		/* The Wi-Fi driver will never generate this event, which, as a result, can be ignored by the application event
		 * callback. This event may be removed in future releases. */
		case WIFI_EVENT_WIFI_READY:
			ESP_LOGI(TAG, "WIFI_EVENT_WIFI_READY");
			break;

		/* The scan-done event is triggered by esp_wifi_scan_start() and will arise in the following scenarios:
			  The scan is completed, e.g., the target AP is found successfully, or all channels have been scanned.
			  The scan is stopped by esp_wifi_scan_stop().
			  The esp_wifi_scan_start() is called before the scan is completed. A new scan will override the current
				 scan and a scan-done event will be generated.
			The scan-done event will not arise in the following scenarios:
			  It is a blocked scan.
			  The scan is caused by esp_wifi_connect().
			Upon receiving this event, the event task does nothing. The application event callback needs to call
			esp_wifi_scan_get_ap_num() and esp_wifi_scan_get_ap_records() to fetch the scanned AP list and trigger
			the Wi-Fi driver to free the internal memory which is allocated during the scan (do not forget to do this)!
		 */
		case WIFI_EVENT_SCAN_DONE:
			ESP_LOGD(TAG, "WIFI_EVENT_SCAN_DONE");
	    	xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
			wifi_event_sta_scan_done_t* event_sta_scan_done = (wifi_event_sta_scan_done_t*)malloc(sizeof(wifi_event_sta_scan_done_t));
			*event_sta_scan_done = *((wifi_event_sta_scan_done_t*)event_data);
	    	wifi_manager_send_message(WM_EVENT_SCAN_DONE, event_sta_scan_done);
			break;

		/* If esp_wifi_start() returns ESP_OK and the current Wi-Fi mode is Station or AP+Station, then this event will
		 * arise. Upon receiving this event, the event task will initialize the LwIP network interface (netif).
		 * Generally, the application event callback needs to call esp_wifi_connect() to connect to the configured AP. */
		case WIFI_EVENT_STA_START:
			ESP_LOGI(TAG, "");
			break;

		/* If esp_wifi_stop() returns ESP_OK and the current Wi-Fi mode is Station or AP+Station, then this event will arise.
		 * Upon receiving this event, the event task will release the station’s IP address, stop the DHCP client, remove
		 * TCP/UDP-related connections and clear the LwIP station netif, etc. The application event callback generally does
		 * not need to do anything. */
		case WIFI_EVENT_STA_STOP:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
			break;

		/* If esp_wifi_connect() returns ESP_OK and the station successfully connects to the target AP, the connection event
		 * will arise. Upon receiving this event, the event task starts the DHCP client and begins the DHCP process of getting
		 * the IP address. Then, the Wi-Fi driver is ready for sending and receiving data. This moment is good for beginning
		 * the application work, provided that the application does not depend on LwIP, namely the IP address. However, if
		 * the application is LwIP-based, then you need to wait until the got ip event comes in. */
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
			break;

		/* This event can be generated in the following scenarios:
		 *
		 *     When esp_wifi_disconnect(), or esp_wifi_stop(), or esp_wifi_deinit(), or esp_wifi_restart() is called and
		 *     the station is already connected to the AP.
		 *
		 *     When esp_wifi_connect() is called, but the Wi-Fi driver fails to set up a connection with the AP due to certain
		 *     reasons, e.g. the scan fails to find the target AP, authentication times out, etc. If there are more than one AP
		 *     with the same SSID, the disconnected event is raised after the station fails to connect all of the found APs.
		 *
		 *     When the Wi-Fi connection is disrupted because of specific reasons, e.g., the station continuously loses N beacons,
		 *     the AP kicks off the station, the AP’s authentication mode is changed, etc.
		 *
		 * Upon receiving this event, the default behavior of the event task is: - Shuts down the station’s LwIP netif.
		 * - Notifies the LwIP task to clear the UDP/TCP connections which cause the wrong status to all sockets. For socket-based
		 * applications, the application callback can choose to close all sockets and re-create them, if necessary, upon receiving
		 * this event.
		 *
		 * The most common event handle code for this event in application is to call esp_wifi_connect() to reconnect the Wi-Fi.
		 * However, if the event is raised because esp_wifi_disconnect() is called, the application should not call esp_wifi_connect()
		 * to reconnect. It’s application’s responsibility to distinguish whether the event is caused by esp_wifi_disconnect() or
		 * other reasons. Sometimes a better reconnect strategy is required, refer to <Wi-Fi Reconnect> and
		 * <Scan When Wi-Fi Is Connecting>.
		 *
		 * Another thing deserves our attention is that the default behavior of LwIP is to abort all TCP socket connections on
		 * receiving the disconnect. Most of time it is not a problem. However, for some special application, this may not be
		 * what they want, consider following scenarios:
		 *
		 *    The application creates a TCP connection to maintain the application-level keep-alive data that is sent out
		 *    every 60 seconds.
		 *
		 *    Due to certain reasons, the Wi-Fi connection is cut off, and the <WIFI_EVENT_STA_DISCONNECTED> is raised.
		 *    According to the current implementation, all TCP connections will be removed and the keep-alive socket will be
		 *    in a wrong status. However, since the application designer believes that the network layer should NOT care about
		 *    this error at the Wi-Fi layer, the application does not close the socket.
		 *
		 *    Five seconds later, the Wi-Fi connection is restored because esp_wifi_connect() is called in the application
		 *    event callback function. Moreover, the station connects to the same AP and gets the same IPV4 address as before.
		 *
		 *    Sixty seconds later, when the application sends out data with the keep-alive socket, the socket returns an error
		 *    and the application closes the socket and re-creates it when necessary.
		 *
		 * In above scenario, ideally, the application sockets and the network layer should not be affected, since the Wi-Fi
		 * connection only fails temporarily and recovers very quickly. The application can enable “Keep TCP connections when
		 * IP changed” via LwIP menuconfig.*/
		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED");

			wifi_event_sta_disconnected_t* wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t*)malloc(sizeof(wifi_event_sta_disconnected_t));
			*wifi_event_sta_disconnected =  *( (wifi_event_sta_disconnected_t*)event_data );

			/* if a DISCONNECT message is posted while a scan is in progress this scan will NEVER end, causing scan to never work again. For this reason SCAN_BIT is cleared too */
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_SCAN_BIT);

			/* post disconnect event with reason code */
			wifi_manager_send_message(WM_EVENT_STA_DISCONNECTED, (void*)wifi_event_sta_disconnected );
			break;

		/* This event arises when the AP to which the station is connected changes its authentication mode, e.g., from no auth
		 * to WPA. Upon receiving this event, the event task will do nothing. Generally, the application event callback does
		 * not need to handle this either. */
		case WIFI_EVENT_STA_AUTHMODE_CHANGE:
			ESP_LOGI(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGE");
			break;

		case WIFI_EVENT_AP_START:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
			xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
			break;

		case WIFI_EVENT_AP_STOP:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
			xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED_BIT);
			break;

		/* Every time a station is connected to ESP32 AP, the <WIFI_EVENT_AP_STACONNECTED> will arise. Upon receiving this
		 * event, the event task will do nothing, and the application callback can also ignore it. However, you may want
		 * to do something, for example, to get the info of the connected STA, etc. */
		case WIFI_EVENT_AP_STACONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
			break;

		/* This event can happen in the following scenarios:
		 *   The application calls esp_wifi_disconnect(), or esp_wifi_deauth_sta(), to manually disconnect the station.
		 *   The Wi-Fi driver kicks off the station, e.g. because the AP has not received any packets in the past five minutes, etc.
		 *   The station kicks off the AP.
		 * When this event happens, the event task will do nothing, but the application event callback needs to do
		 * something, e.g., close the socket which is related to this station, etc. */
		case WIFI_EVENT_AP_STADISCONNECTED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
			break;

		/* This event is disabled by default. The application can enable it via API esp_wifi_set_event_mask().
		 * When this event is enabled, it will be raised each time the AP receives a probe request. */
		case WIFI_EVENT_AP_PROBEREQRECVED:
			ESP_LOGI(TAG, "WIFI_EVENT_AP_PROBEREQRECVED");
			break;

		} /* end switch */
	}
	else if(event_base == IP_EVENT){

		switch(event_id){

		/* This event arises when the DHCP client successfully gets the IPV4 address from the DHCP server,
		 * or when the IPV4 address is changed. The event means that everything is ready and the application can begin
		 * its tasks (e.g., creating sockets).
		 * The IPV4 may be changed because of the following reasons:
		 *    The DHCP client fails to renew/rebind the IPV4 address, and the station’s IPV4 is reset to 0.
		 *    The DHCP client rebinds to a different address.
		 *    The static-configured IPV4 address is changed.
		 * Whether the IPV4 address is changed or NOT is indicated by field ip_change of ip_event_got_ip_t.
		 * The socket is based on the IPV4 address, which means that, if the IPV4 changes, all sockets relating to this
		 * IPV4 will become abnormal. Upon receiving this event, the application needs to close all sockets and recreate
		 * the application when the IPV4 changes to a valid one. */
		case IP_EVENT_STA_GOT_IP:
			ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
	        xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
	        ip_event_got_ip_t* ip_event_got_ip = (ip_event_got_ip_t*)malloc(sizeof(ip_event_got_ip_t));
			*ip_event_got_ip =  *( (ip_event_got_ip_t*)event_data );
	        wifi_manager_send_message(WM_EVENT_STA_GOT_IP, (void*)(ip_event_got_ip) );
			break;

		/* This event arises when the IPV6 SLAAC support auto-configures an address for the ESP32, or when this address changes.
		 * The event means that everything is ready and the application can begin its tasks (e.g., creating sockets). */
		case IP_EVENT_GOT_IP6:
			ESP_LOGI(TAG, "IP_EVENT_GOT_IP6");
			break;

		/* This event arises when the IPV4 address become invalid.
		 * IP_STA_LOST_IP doesn’t arise immediately after the WiFi disconnects, instead it starts an IPV4 address lost timer,
		 * if the IPV4 address is got before ip lost timer expires, IP_EVENT_STA_LOST_IP doesn’t happen. Otherwise, the event
		 * arises when IPV4 address lost timer expires.
		 * Generally the application don’t need to care about this event, it is just a debug event to let the application
		 * know that the IPV4 address is lost. */
		case IP_EVENT_STA_LOST_IP:
			ESP_LOGI(TAG, "IP_EVENT_STA_LOST_IP");
			break;

		}
	}

}


wifi_config_t* wifi_manager_get_wifi_sta_config(){
	return wifi_manager_config_sta;
}


void wifi_manager_connect_async(){
	/* in order to avoid a false positive on the front end app we need to quickly flush the ip json
	 * There'se a risk the front end sees an IP or a password error when in fact
	 * it's a remnant from a previous connection
	 */
	if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
		wifi_manager_clear_ip_info_json();
		wifi_manager_unlock_json_buffer();
	}
	wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_USER);
}


char* wifi_manager_get_ip_info_json(){
	return ip_info_json;
}


void wifi_manager_destroy(){

	vTaskDelete(task_wifi_manager);
	task_wifi_manager = NULL;

	/* heap buffers */
	free(accessp_records);
	accessp_records = NULL;
	free(accessp_json);
	accessp_json = NULL;
	free(ip_info_json);
	ip_info_json = NULL;
	free(wifi_manager_sta_ip);
	wifi_manager_sta_ip = NULL;
	if(wifi_manager_config_sta){
		free(wifi_manager_config_sta);
		wifi_manager_config_sta = NULL;
	}

	/* RTOS objects */
	vSemaphoreDelete(wifi_manager_json_mutex);
	wifi_manager_json_mutex = NULL;
	vSemaphoreDelete(wifi_manager_sta_ip_mutex);
	wifi_manager_sta_ip_mutex = NULL;
	vEventGroupDelete(wifi_manager_event_group);
	wifi_manager_event_group = NULL;
	vQueueDelete(wifi_manager_queue);
	wifi_manager_queue = NULL;


}


void wifi_manager_filter_unique( wifi_ap_record_t * aplist, uint16_t * aps) {
	int total_unique;
	wifi_ap_record_t * first_free;
	total_unique=*aps;

	first_free=NULL;

	for(int i=0; i<*aps-1;i++) {
		wifi_ap_record_t * ap = &aplist[i];

		/* skip the previously removed APs */
		if (ap->ssid[0] == 0) continue;

		/* remove the identical SSID+authmodes */
		for(int j=i+1; j<*aps;j++) {
			wifi_ap_record_t * ap1 = &aplist[j];
			if ( (strcmp((const char *)ap->ssid, (const char *)ap1->ssid)==0) && 
			     (ap->authmode == ap1->authmode) ) { /* same SSID, different auth mode is skipped */
				/* save the rssi for the display */
				if ((ap1->rssi) > (ap->rssi)) ap->rssi=ap1->rssi;
				/* clearing the record */
				memset(ap1,0, sizeof(wifi_ap_record_t));
			}
		}
	}
	/* reorder the list so APs follow each other in the list */
	for(int i=0; i<*aps;i++) {
		wifi_ap_record_t * ap = &aplist[i];
		/* skipping all that has no name */
		if (ap->ssid[0] == 0) {
			/* mark the first free slot */
			if (first_free==NULL) first_free=ap;
			total_unique--;
			continue;
		}
		if (first_free!=NULL) {
			memcpy(first_free, ap, sizeof(wifi_ap_record_t));
			memset(ap,0, sizeof(wifi_ap_record_t));
			/* find the next free slot */
			for(int j=0; j<*aps;j++) {
				if (aplist[j].ssid[0]==0) {
					first_free=&aplist[j];
					break;
				}
			}
		}
	}
	/* update the length of the list */
	*aps = total_unique;
}


BaseType_t wifi_manager_send_message_to_front(message_code_t code, void *param){
	queue_message msg;
	msg.code = code;
	msg.param = param;
	return xQueueSendToFront( wifi_manager_queue, &msg, portMAX_DELAY);
}

BaseType_t wifi_manager_send_message(message_code_t code, void *param){
	queue_message msg;
	msg.code = code;
	msg.param = param;
	return xQueueSend( wifi_manager_queue, &msg, portMAX_DELAY);
}


void wifi_manager_set_callback(message_code_t message_code, void (*func_ptr)(void*) ){

	if(cb_ptr_arr && message_code < WM_MESSAGE_CODE_COUNT){
		cb_ptr_arr[message_code] = func_ptr;
	}
}

esp_netif_t* wifi_manager_get_esp_netif_ap(){
	return esp_netif_ap;
}

esp_netif_t* wifi_manager_get_esp_netif_sta(){
	return esp_netif_sta;
}

void wifi_manager( void * pvParameters ){


	queue_message msg;
	BaseType_t xStatus;
	EventBits_t uxBits;
	uint8_t	retries = 0;


	/* initialize the tcp stack */
	ESP_ERROR_CHECK(esp_netif_init());

	/* event loop for the wifi driver */
	ESP_ERROR_CHECK(esp_event_loop_create_default());

	esp_netif_sta = esp_netif_create_default_wifi_sta();
	esp_netif_ap = esp_netif_create_default_wifi_ap();


	/* default wifi config */
	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

	/* event handler for the connection */
    esp_event_handler_instance_t instance_wifi_event;
    esp_event_handler_instance_t instance_ip_event;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL,&instance_wifi_event));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &wifi_manager_event_handler, NULL,&instance_ip_event));


	/* SoftAP - Wifi Access Point configuration setup */
	wifi_config_t ap_config = {
		.ap = {
			.ssid_len = 0,
			.channel = wifi_settings.ap_channel,
			.ssid_hidden = wifi_settings.ap_ssid_hidden,
			.max_connection = DEFAULT_AP_MAX_CONNECTIONS,
			.beacon_interval = DEFAULT_AP_BEACON_INTERVAL,
		},
	};
	memcpy(ap_config.ap.ssid, wifi_settings.ap_ssid , sizeof(wifi_settings.ap_ssid));

	/* if the password lenght is under 8 char which is the minium for WPA2, the access point starts as open */
	if(strlen( (char*)wifi_settings.ap_pwd) < WPA2_MINIMUM_PASSWORD_LENGTH){
		ap_config.ap.authmode = WIFI_AUTH_OPEN;
		memset( ap_config.ap.password, 0x00, sizeof(ap_config.ap.password) );
	}
	else{
		ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
		memcpy(ap_config.ap.password, wifi_settings.ap_pwd, sizeof(wifi_settings.ap_pwd));
	}
	

	/* DHCP AP configuration */
	esp_netif_dhcps_stop(esp_netif_ap); /* DHCP client/server must be stopped before setting new IP information. */
	esp_netif_ip_info_t ap_ip_info;
	memset(&ap_ip_info, 0x00, sizeof(ap_ip_info));
	inet_pton(AF_INET, DEFAULT_AP_IP, &ap_ip_info.ip);
	inet_pton(AF_INET, DEFAULT_AP_GATEWAY, &ap_ip_info.gw);
	inet_pton(AF_INET, DEFAULT_AP_NETMASK, &ap_ip_info.netmask);
	ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ap_ip_info));
	ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));

	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config));
	ESP_ERROR_CHECK(esp_wifi_set_bandwidth(WIFI_IF_AP, wifi_settings.ap_bandwidth));
	ESP_ERROR_CHECK(esp_wifi_set_ps(wifi_settings.sta_power_save));


	/* by default the mode is STA because wifi_manager will not start the access point unless it has to! */
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	/* start http server */
	http_app_start(false);

	/* wifi scanner config */
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
		.show_hidden = true
	};

	/* enqueue first event: load previous config */
	wifi_manager_send_message(WM_ORDER_LOAD_AND_RESTORE_STA, NULL);


	/* main processing loop */
	for(;;){
		xStatus = xQueueReceive( wifi_manager_queue, &msg, portMAX_DELAY );

		if( xStatus == pdPASS ){
			switch(msg.code){

			case WM_EVENT_SCAN_DONE:{
				wifi_event_sta_scan_done_t *evt_scan_done = (wifi_event_sta_scan_done_t*)msg.param;
				/* only check for AP if the scan is succesful */
				if(evt_scan_done->status == 0){
					/* As input param, it stores max AP number ap_records can hold. As output param, it receives the actual AP number this API returns.
					* As a consequence, ap_num MUST be reset to MAX_AP_NUM at every scan */
					ap_num = MAX_AP_NUM;
					ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, accessp_records));
					/* make sure the http server isn't trying to access the list while it gets refreshed */
					if(wifi_manager_lock_json_buffer( pdMS_TO_TICKS(1000) )){
						/* Will remove the duplicate SSIDs from the list and update ap_num */
						wifi_manager_filter_unique(accessp_records, &ap_num);
						wifi_manager_generate_acess_points_json();
						wifi_manager_unlock_json_buffer();
					}
					else{
						ESP_LOGE(TAG, "could not get access to json mutex in wifi_scan");
					}
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(evt_scan_done);
				}
				break;

			case WM_ORDER_START_WIFI_SCAN:
				ESP_LOGD(TAG, "MESSAGE: ORDER_START_WIFI_SCAN");

				/* if a scan is already in progress this message is simply ignored thanks to the WIFI_MANAGER_SCAN_BIT uxBit */
				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if(! (uxBits & WIFI_MANAGER_SCAN_BIT) ){
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_SCAN_BIT);
					ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_LOAD_AND_RESTORE_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_LOAD_AND_RESTORE_STA");
				if(wifi_manager_fetch_wifi_sta_config()){
					ESP_LOGI(TAG, "Saved wifi found on startup. Will attempt to connect.");
					wifi_manager_send_message(WM_ORDER_CONNECT_STA, (void*)CONNECTION_REQUEST_RESTORE_CONNECTION);
				}
				else{
					/* no wifi saved: start soft AP! This is what should happen during a first run */
					ESP_LOGI(TAG, "No saved wifi found on startup. Starting access point.");
					wifi_manager_send_message(WM_ORDER_START_AP, NULL);
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_CONNECT_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_CONNECT_STA");

				/* very important: precise that this connection attempt is specifically requested.
				 * Param in that case is a boolean indicating if the request was made automatically
				 * by the wifi_manager.
				 * */
				if((BaseType_t)msg.param == CONNECTION_REQUEST_USER) {
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
				}
				else if((BaseType_t)msg.param == CONNECTION_REQUEST_RESTORE_CONNECTION) {
					xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
				}

				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if( ! (uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT) ){
					/* update config to latest and attempt connection */
					ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, wifi_manager_get_wifi_sta_config()));

					/* if there is a wifi scan in progress abort it first
					   Calling esp_wifi_scan_stop will trigger a SCAN_DONE event which will reset this bit */
					if(uxBits & WIFI_MANAGER_SCAN_BIT){
						esp_wifi_scan_stop();
					}
					ESP_ERROR_CHECK(esp_wifi_connect());
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_EVENT_STA_DISCONNECTED:
				;wifi_event_sta_disconnected_t* wifi_event_sta_disconnected = (wifi_event_sta_disconnected_t*)msg.param;
				ESP_LOGI(TAG, "MESSAGE: EVENT_STA_DISCONNECTED with Reason code: %d", wifi_event_sta_disconnected->reason);

				/* this even can be posted in numerous different conditions
				 *
				 * 1. SSID password is wrong
				 * 2. Manual disconnection ordered
				 * 3. Connection lost
				 *
				 * Having clear understand as to WHY the event was posted is key to having an efficient wifi manager
				 *
				 * With wifi_manager, we determine:
				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, We consider it's a client that requested the connection.
				 *    When SYSTEM_EVENT_STA_DISCONNECTED is posted, it's probably a password/something went wrong with the handshake.
				 *
				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT is set, it's a disconnection that was ASKED by the client (clicking disconnect in the app)
				 *    When SYSTEM_EVENT_STA_DISCONNECTED is posted, saved wifi is erased from the NVS memory.
				 *
				 *  If WIFI_MANAGER_REQUEST_STA_CONNECT_BIT and WIFI_MANAGER_REQUEST_STA_CONNECT_BIT are NOT set, it's a lost connection
				 *
				 *  In this version of the software, reason codes are not used. They are indicated here for potential future usage.
				 *
				 *  REASON CODE:
				 *  1		UNSPECIFIED
				 *  2		AUTH_EXPIRE					auth no longer valid, this smells like someone changed a password on the AP
				 *  3		AUTH_LEAVE
				 *  4		ASSOC_EXPIRE
				 *  5		ASSOC_TOOMANY				too many devices already connected to the AP => AP fails to respond
				 *  6		NOT_AUTHED
				 *  7		NOT_ASSOCED
				 *  8		ASSOC_LEAVE					tested as manual disconnect by user OR in the wireless MAC blacklist
				 *  9		ASSOC_NOT_AUTHED
				 *  10		DISASSOC_PWRCAP_BAD
				 *  11		DISASSOC_SUPCHAN_BAD
				 *	12		<n/a>
				 *  13		IE_INVALID
				 *  14		MIC_FAILURE
				 *  15		4WAY_HANDSHAKE_TIMEOUT		wrong password! This was personnaly tested on my home wifi with a wrong password.
				 *  16		GROUP_KEY_UPDATE_TIMEOUT
				 *  17		IE_IN_4WAY_DIFFERS
				 *  18		GROUP_CIPHER_INVALID
				 *  19		PAIRWISE_CIPHER_INVALID
				 *  20		AKMP_INVALID
				 *  21		UNSUPP_RSN_IE_VERSION
				 *  22		INVALID_RSN_IE_CAP
				 *  23		802_1X_AUTH_FAILED			wrong password?
				 *  24		CIPHER_SUITE_REJECTED
				 *  200		BEACON_TIMEOUT
				 *  201		NO_AP_FOUND
				 *  202		AUTH_FAIL
				 *  203		ASSOC_FAIL
				 *  204		HANDSHAKE_TIMEOUT
				 *
				 * */

				/* reset saved sta IP */
				wifi_manager_safe_update_sta_ip_string((uint32_t)0);

				/* if there was a timer on to stop the AP, well now it's time to cancel that since connection was lost! */
				if(xTimerIsTimerActive(wifi_manager_shutdown_ap_timer) == pdTRUE ){
					xTimerStop( wifi_manager_shutdown_ap_timer, (TickType_t)0 );
				}

				uxBits = xEventGroupGetBits(wifi_manager_event_group);
				if( uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT ){
					/* there are no retries when it's a user requested connection by design. This avoids a user hanging too much
					 * in case they typed a wrong password for instance. Here we simply clear the request bit and move on */
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_FAILED_ATTEMPT );
						wifi_manager_unlock_json_buffer();
					}

				}
				else if (uxBits & WIFI_MANAGER_REQUEST_DISCONNECT_BIT){
					/* user manually requested a disconnect so the lost connection is a normal event. Clear the flag and restart the AP */
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

					/* erase configuration */
					if(wifi_manager_config_sta){
						memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));
					}

					/* regenerate json status */
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_USER_DISCONNECT );
						wifi_manager_unlock_json_buffer();
					}

					/* save NVS memory */
					wifi_manager_save_sta_config();

					/* start SoftAP */
					wifi_manager_send_message(WM_ORDER_START_AP, NULL);
				}
				else{
					/* lost connection ? */
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json( UPDATE_LOST_CONNECTION );
						wifi_manager_unlock_json_buffer();
					}

					/* Start the timer that will try to restore the saved config */
					xTimerStart( wifi_manager_retry_timer, (TickType_t)0 );

					/* if it was a restore attempt connection, we clear the bit */
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);

					/* if the AP is not started, we check if we have reached the threshold of failed attempt to start it */
					if(! (uxBits & WIFI_MANAGER_AP_STARTED_BIT) ){

						/* if the nunber of retries is below the threshold to start the AP, a reconnection attempt is made
						 * This way we avoid restarting the AP directly in case the connection is mementarily lost */
						if(retries < WIFI_MANAGER_MAX_RETRY_START_AP){
							retries++;
						}
						else{
							/* In this scenario the connection was lost beyond repair: kick start the AP! */
							retries = 0;

							/* start SoftAP */
							wifi_manager_send_message(WM_ORDER_START_AP, NULL);
						}
					}
				}

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(wifi_event_sta_disconnected);

				break;

			case WM_ORDER_START_AP:
				ESP_LOGI(TAG, "MESSAGE: ORDER_START_AP");

				ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

				/* restart HTTP daemon */
				http_app_stop();
				http_app_start(true);

				/* start DNS */
				dns_server_start();

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			case WM_ORDER_STOP_AP:
				ESP_LOGI(TAG, "MESSAGE: ORDER_STOP_AP");


				uxBits = xEventGroupGetBits(wifi_manager_event_group);

				/* before stopping the AP, we check that we are still connected. There's a chance that once the timer
				 * kicks in, for whatever reason the esp32 is already disconnected.
				 */
				if(uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT){

					/* set to STA only */
					esp_wifi_set_mode(WIFI_MODE_STA);

					/* stop DNS */
					dns_server_stop();

					/* restart HTTP daemon */
					http_app_stop();
					http_app_start(false);

					/* callback */
					if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);
				}

				break;

			case WM_EVENT_STA_GOT_IP:
				ESP_LOGI(TAG, "WM_EVENT_STA_GOT_IP");
				ip_event_got_ip_t* ip_event_got_ip = (ip_event_got_ip_t*)msg.param; 
				uxBits = xEventGroupGetBits(wifi_manager_event_group);

				/* reset connection requests bits -- doesn't matter if it was set or not */
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);

				/* save IP as a string for the HTTP server host */
				wifi_manager_safe_update_sta_ip_string(ip_event_got_ip->ip_info.ip.addr);

				/* save wifi config in NVS if it wasn't a restored of a connection */
				if(uxBits & WIFI_MANAGER_REQUEST_RESTORE_STA_BIT){
					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_RESTORE_STA_BIT);
				}
				else{
					wifi_manager_save_sta_config();
				}

				/* reset number of retries */
				retries = 0;

				/* refresh JSON with the new IP */
				if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
					/* generate the connection info with success */
					wifi_manager_generate_ip_info_json( UPDATE_CONNECTION_OK );
					wifi_manager_unlock_json_buffer();
				}
				else { abort(); }

				/* bring down DNS hijack */
				dns_server_stop();

				/* start the timer that will eventually shutdown the access point
				 * We check first that it's actually running because in case of a boot and restore connection
				 * the AP is not even started to begin with.
				 */
				if(uxBits & WIFI_MANAGER_AP_STARTED_BIT){
					TickType_t t = pdMS_TO_TICKS( WIFI_MANAGER_SHUTDOWN_AP_TIMER );

					/* if for whatever reason user configured the shutdown timer to be less than 1 tick, the AP is stopped straight away */
					if(t > 0){
						xTimerStart( wifi_manager_shutdown_ap_timer, (TickType_t)0 );
					}
					else{
						wifi_manager_send_message(WM_ORDER_STOP_AP, (void*)NULL);
					}

				}

				/* callback and free memory allocated for the void* param */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])( msg.param );
				free(ip_event_got_ip);

				break;

			case WM_ORDER_DISCONNECT_STA:
				ESP_LOGI(TAG, "MESSAGE: ORDER_DISCONNECT_STA");

				/* precise this is coming from a user request */
				xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_DISCONNECT_BIT);

				/* order wifi discconect */
				ESP_ERROR_CHECK(esp_wifi_disconnect());

				/* callback */
				if(cb_ptr_arr[msg.code]) (*cb_ptr_arr[msg.code])(NULL);

				break;

			default:
				break;

			} /* end of switch/case */
		} /* end of if status=pdPASS */
	} /* end of for loop */

	vTaskDelete( NULL );

}


