/*
Copyright (c) 2017 Tony Pottier

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
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"

#include "json.h"
#include "http_server.h"
#include "wifi_manager.h"




SemaphoreHandle_t wifi_manager_json_mutex = NULL;
uint16_t ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records; //[MAX_AP_NUM];
char *accessp_json = NULL;
char *ip_info_json = NULL;
wifi_config_t* wifi_manager_config_sta = NULL;


EventGroupHandle_t wifi_manager_event_group;
const int WIFI_MANAGER_WIFI_CONNECTED_BIT = BIT0;
const int WIFI_MANAGER_AP_STA_CONNECTED_BIT = BIT1;
const int WIFI_MANAGER_AP_STARTED = BIT2;
const int WIFI_MANAGER_REQUEST_STA_CONNECT_BIT = BIT3;
const int WIFI_MANAGER_STA_DISCONNECT_BIT = BIT4;
const int WIFI_MANAGER_REQUEST_WIFI_SCAN = BIT5;

//wifi_config_t wifi_config;

void wifi_manager_scan_async(){
	xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_SCAN);
}

void set_wifi_sta_config(char *ssid, char *password){
	strcpy((char*)wifi_manager_config_sta->sta.ssid, (char*)ssid);
	strcpy((char*)wifi_manager_config_sta->sta.password, (char*)password);
}

uint8_t wifi_manager_fetch_wifi_sta_config(){

	nvs_handle handle;
	esp_err_t esp_err;
	if(nvs_open("espwifimgr", NVS_READONLY, &handle) == ESP_OK){
		size_t sz;

		//ssid
		esp_err = nvs_get_str(handle, "ssid", NULL, &sz);
		if(esp_err != ESP_OK) return NULL;
		uint8_t* ssid = malloc(sz);
		esp_err = nvs_get_str(handle, "ssid", (char*)ssid, &sz);

		//password
		esp_err = nvs_get_str(handle, "password", NULL, &sz);
		if(esp_err != ESP_OK) {free(ssid); return NULL;}
		uint8_t* password = malloc(sz);
		esp_err = nvs_get_str(handle, "password", (char*)password, &sz);


		if(wifi_manager_config_sta == NULL)
			wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));

		strcpy((char*)wifi_manager_config_sta->sta.ssid, (char*)ssid);
		strcpy((char*)wifi_manager_config_sta->sta.password, (char*)ssid);

		free(ssid);
		free(password);

		nvs_close(handle);

		return pdTRUE;

	}
	else{
		return pdFALSE;
	}

}










void wifi_manager_generate_ip_info_json(){

	EventBits_t uxBits = xEventGroupGetBits(wifi_manager_event_group);
	wifi_config_t *config = wifi_manager_get_wifi_sta_config();
	if( (uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT) && config){

		memset(ip_info_json, 0x00, JSON_IP_INFO_SIZE);

		tcpip_adapter_ip_info_t ip_info;
		ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

		/* to avoid declaring a new buffer we copy the data directly into the buffer at its correct address */
		strcpy(ip_info_json, "{\"ssid\":");
		json_print_string(config->sta.ssid,  (unsigned char*)(ip_info_json+strlen(ip_info_json)) );

		/* rest of the information is copied after the ssid */
		const char ip_info_json_format[] = ",\"ip\":\"%s\",\"netmask\":\"%s\",\"gw\":\"%s\"}\n";
		char ip[IP4ADDR_STRLEN_MAX]; /* note: IP4ADDR_STRLEN_MAX is defined in lwip */
		char gw[IP4ADDR_STRLEN_MAX];
		char netmask[IP4ADDR_STRLEN_MAX];

		strcpy(ip, ip4addr_ntoa(&ip_info.ip));
		strcpy(netmask, ip4addr_ntoa(&ip_info.netmask));
		strcpy(gw, ip4addr_ntoa(&ip_info.gw));

		sprintf( (ip_info_json + strlen(ip_info_json)), ip_info_json_format,
				ip,
				netmask,
				gw);
	}
	else{
		strcpy(ip_info_json, "{}\n");
	}


}

void wifi_manager_generate_acess_points_json(){

	strcpy(accessp_json, "[\n");


	const char oneap_str[] = "{\"ssid\":\"%s\",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%c\n";

	for(int i=0; i<ap_num;i++){

		char one_ap[JSON_ONE_APP_SIZE];

		wifi_ap_record_t ap = accessp_records[i];
		sprintf(one_ap, oneap_str,
				json_escape_string((char*)ap.ssid),
				ap.primary,
				ap.rssi,
				ap.authmode,
				i==ap_num-1?' ':',');
		strcat (accessp_json, one_ap);
	}

	strcat(accessp_json, "]");
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


esp_err_t wifi_manager_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {

    case SYSTEM_EVENT_AP_START:
    	xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED);
		break;

    case SYSTEM_EVENT_AP_STACONNECTED:
		xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
		break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
    	xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_AP_STA_CONNECTED_BIT);
		break;

    case SYSTEM_EVENT_STA_START:
        //esp_wifi_connect();
        break;

	case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
        break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
		xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT);
		xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT);
        break;

	default:
        break;
    }
	return ESP_OK;
}






wifi_config_t* wifi_manager_get_wifi_sta_config(){
	return wifi_manager_config_sta;
}


void wifi_manager_connect_async(){
	xEventGroupSetBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
}


char* wifi_manager_get_ip_info_json(){
	return ip_info_json;
}


void wifi_manager_destroy(){

	/* buffers */
	free(accessp_records);
	accessp_records = NULL;
	free(accessp_json);
	accessp_json = NULL;
	free(ip_info_json);
	ip_info_json = NULL;

	/* RTOS objects */
	vSemaphoreDelete(wifi_manager_json_mutex);
	wifi_manager_json_mutex = NULL;
	vEventGroupDelete(wifi_manager_event_group);

	if(wifi_manager_config_sta){
		free(wifi_manager_config_sta);
		wifi_manager_config_sta = NULL;
	}
}

void wifi_manager( void * pvParameters ){

	/* memory allocation of objects used by the task */
	wifi_manager_json_mutex = xSemaphoreCreateMutex();
	accessp_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * MAX_AP_NUM);
	accessp_json = (char*)malloc(ap_num * JSON_ONE_APP_SIZE + 4); //4 bytes for json encapsulation of "[\n" and "]\0"
	strcpy(accessp_json, "[]\n");
	ip_info_json = (char*)malloc(sizeof(char) * JSON_IP_INFO_SIZE);
	strcpy(ip_info_json, "{}\n");
	wifi_manager_config_sta = (wifi_config_t*)malloc(sizeof(wifi_config_t));
	memset(wifi_manager_config_sta, 0x00, sizeof(wifi_config_t));

	/* initialize the tcp stack */
	tcpip_adapter_init();

    /* event handler and event group for the wifi driver */
	wifi_manager_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_manager_event_handler, NULL));

    //wifi scanner config
	wifi_scan_config_t scan_config = {
		.ssid = 0,
		.bssid = 0,
		.channel = 0,
		.show_hidden = true
	};


	//try to get access to previously saved wifi
	if(wifi_manager_fetch_wifi_sta_config()){
#if WIFI_MANAGER_DEBUG
		printf("wifi_manager: saved wifi found on startup\n");
#endif
	}
	else{

		//int i = ESP_ERR_WIFI_BASE;
#if WIFI_MANAGER_DEBUG
		printf("wifi_manager: could not find saved wifi in flash storage\n");
#endif
		//start the an access point

		//stop dhcp
		ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));

		// assign a static IP to the network interface
		tcpip_adapter_ip_info_t info;
	    memset(&info, 0x00, sizeof(info));
		IP4_ADDR(&info.ip, 192, 168, 1, 1);
	    IP4_ADDR(&info.gw, 192, 168, 1, 1);
	    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
	    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));

	    //start dhcp
	    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

	    //init wifi as access point
		wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
		ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

		// configure the wifi connection and start the interface
		wifi_config_t ap_config = {
			.ap = {
				.ssid = AP_SSID,
				.password = AP_PASSWORD,
				.ssid_len = 0,
				.channel = AP_CHANNEL,
				.authmode = WIFI_AUTH_WPA2_PSK,//WIFI_AUTH_OPEN, //CONFIG_AP_AUTHMODE,
				.ssid_hidden = AP_SSID_HIDDEN,
				.max_connection = AP_MAX_CONNECTIONS,
				.beacon_interval = AP_BEACON_INTERVAL,
			},
		};
		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
		ESP_ERROR_CHECK(esp_wifi_start());

#if WIFI_MANAGER_DEBUG
		printf("wifi_mamager: starting softAP with ssid %s\n", ap_config.ap.ssid);
#endif

		//wait for access point to start
		xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_AP_STARTED, pdFALSE, pdTRUE, portMAX_DELAY );

#if WIFI_MANAGER_DEBUG
		printf("wifi_mamager: softAP started, starting http_server\n");
#endif
		http_server_set_event_start();


		//keep scanning wifis until someone tries to connect to an AP
		EventBits_t uxBits;
		//uint8_t tick = 0;
		for(;;){

			/* someone tries to make a connection? if so: connect! */
			uxBits = xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT | WIFI_MANAGER_REQUEST_WIFI_SCAN, pdFALSE, pdFALSE, portMAX_DELAY );
			if(uxBits & WIFI_MANAGER_REQUEST_STA_CONNECT_BIT){
				//someone requested a connection!

				/* first thing: if the esp32 is already connected to a access point: disconnect */
				if( (uxBits & WIFI_MANAGER_WIFI_CONNECTED_BIT) == (WIFI_MANAGER_WIFI_CONNECTED_BIT) ){

					xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT);
					ESP_ERROR_CHECK(esp_wifi_disconnect());

					/* wait until wifi disconnects. From experiments, it seems to take about 150ms to disconnect */
					xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT, pdFALSE, pdTRUE, portMAX_DELAY );
				}

				/* set the new config and connect - reset the disconnect bit first as it is later tested */
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_STA_DISCONNECT_BIT);
				ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, wifi_manager_get_wifi_sta_config()));
				ESP_ERROR_CHECK(esp_wifi_connect());

				/* 2 scenarios here: connection is successful and SYSTEM_EVENT_STA_GOT_IP will be posted
				 * or it's a failure and we get a SYSTEM_EVENT_STA_DISCONNECTED with a reason code.
				 * Note that the reason code is not exploited. For all intent and purposes a failure is a failure.
				 */
				uxBits = xEventGroupWaitBits(wifi_manager_event_group, WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_STA_DISCONNECT_BIT, pdFALSE, pdFALSE, portMAX_DELAY );

				if(uxBits & (WIFI_MANAGER_WIFI_CONNECTED_BIT | WIFI_MANAGER_STA_DISCONNECT_BIT)){

					/* Update the json regardless of connection status.
					 * If connection was succesful an IP will get assigned.
					 */
					if(wifi_manager_lock_json_buffer( portMAX_DELAY )){
						wifi_manager_generate_ip_info_json();
						wifi_manager_unlock_json_buffer();
					}
					else{
						/* Even if someone were to furiously refresh a web resource that needs the json mutex,
						 * it seems impossible that this thread cannot obtain the mutex. Abort here is reasonnable.
						 */
						abort();
					}
				}
				else{
					/* hit portMAX_DELAY limit ? */
					abort();
				}

				/* finally: release the connection request bit */
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_STA_CONNECT_BIT);
			}
			else if(uxBits & WIFI_MANAGER_REQUEST_WIFI_SCAN){


				/* safe guard against overflow */
				if(ap_num > MAX_AP_NUM) ap_num = MAX_AP_NUM;

				ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
				ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, accessp_records));

				/* make sure the http server isn't trying to access the list while it gets refreshed */
				if(wifi_manager_lock_json_buffer( ( TickType_t ) 20 )){
					wifi_manager_generate_acess_points_json();
					wifi_manager_unlock_json_buffer();
				}
				else{
#if WIFI_MANAGER_DEBUG
					printf("Could not get access to xSemaphoreScan in wifi_scan\n");
#endif
				}

				/* finally: release the scan request bit */
				xEventGroupClearBits(wifi_manager_event_group, WIFI_MANAGER_REQUEST_WIFI_SCAN);
			}

			//periodically re-do a wifi scan. Here every 5s.
			//tick++;
			//if(tick == 50) tick = 0;
			//vTaskDelay(100 / portTICK_PERIOD_MS);

		}

	}

}
