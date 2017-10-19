/**
 * \file wifi_manager.c
 * \author Tony Pottier
 * \brief Defines all functions necessary for esp32 to connect to a wifi/scan wifis
 *
 * Contains the freeRTOS task and all necessary support
 *
 * \see https://idyl.io
 * \see https://github.com/tonyp7/esp32-wifi-manager
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "json.h"
#include "wifi_manager.h"




SemaphoreHandle_t xSemaphoreScan = NULL;
uint16_t ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records; //[MAX_AP_NUM];
char *accessp_json;

EventGroupHandle_t wifi_manager_event_group;
const int wifi_manager_WIFI_CONNECTED_BIT = BIT0;



wifi_sta_config_t* get_wifi_sta_config(){

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


		wifi_sta_config_t* config = (wifi_sta_config_t*)malloc(sizeof(wifi_sta_config_t));
		strcpy((char*)config->ssid, (char*)ssid);
		strcpy((char*)config->password, (char*)ssid);

		free(ssid);
		free(password);

		nvs_close(handle);

		return config;

	}
	else{
		return NULL;
	}

}


void wifi_scan_init(){
	xSemaphoreScan = xSemaphoreCreateMutex();
	accessp_records = (wifi_ap_record_t*)malloc(sizeof(wifi_ap_record_t) * MAX_AP_NUM);
	accessp_json = (char*)malloc(ap_num * 80 + 20); //80 bytes is enough for one AP and some leeway
	strcpy(accessp_json, "[]\n");
}

void wifi_scan_destroy(){
	free(accessp_records);
	free(accessp_json);
	vSemaphoreDelete(xSemaphoreScan);
}


void wifi_scan_generate_json(){

	strcpy(accessp_json, "[\n");

	for(int i=0; i<ap_num;i++){

		//32 char (ssid) + channel (2) + rssi (3) + auth mode (1) + null char (1) + JSON fluff
		char one_ap[80];

		wifi_ap_record_t ap = accessp_records[i];
		sprintf(one_ap, "{\"ssid\":\"%s\",\"chan\":%d,\"rssi\":%d,\"auth\":%d}%c\n",
				json_escape_string((char*)ap.ssid),
				ap.primary,
				ap.rssi,
				ap.authmode,
				i==ap_num-1?' ':',');
		//printf("%d-%s\n",i,one_ap);
		strcat (accessp_json, one_ap);
	}

	strcat(accessp_json, "]");

	//printf("\n%s\n", accessp_json);
}


uint8_t wifi_scan_lock_ap_list(){
	if( xSemaphoreTake( xSemaphoreScan, ( TickType_t ) 10 ) == pdTRUE ) {
		return pdTRUE;
	}
	else{
		return pdFALSE;
	}
}
void wifi_scan_unlock_ap_list(){
	xSemaphoreGive( xSemaphoreScan );
}

char* wifi_scan_get_json(){
	return accessp_json;
}


esp_err_t wifi_manager_event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {

    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;

	case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_manager_event_group, wifi_manager_WIFI_CONNECTED_BIT);
        break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
		xEventGroupClearBits(wifi_manager_event_group, wifi_manager_WIFI_CONNECTED_BIT);
        break;

	default:
        break;
    }
	return ESP_OK;
}

void wifi_manager( void * pvParameters ){

	//wifi scanner config
	wifi_scan_config_t scan_config = {
				.ssid = 0,
				.bssid = 0,
				.channel = 0,
				.show_hidden = true
	};

	//try to get access to previously saved wifi
	wifi_sta_config_t* sta_config = get_wifi_sta_config();

	if(sta_config){
		printf("found a saved config\n");
	}
	else{
		printf("there is no saved wifi");
	}




	while(true){

		//safe guard against overflow
		if(ap_num > MAX_AP_NUM) ap_num = MAX_AP_NUM;

		ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
		ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, accessp_records));

		/*
		for(int i=0; i<ap_num;i++){
			wifi_ap_record_t ap = accessp_records[i];

			printf("{ssid:%s,chan:%d,rssi:%d,auth:%d}%c\n",
					ap.ssid,
					ap.primary,
					ap.rssi,
					ap.authmode,
					i==ap_num-1?' ':',');
		}*/


		//make sure the http server isn't trying to access the list while it gets refreshed
		if(wifi_scan_lock_ap_list()){
			wifi_scan_generate_json();
			wifi_scan_unlock_ap_list();
		}
		else{
			printf("Could not get access to xSemaphoreScan in wifi_scan\n");
		}


		//wait between each new scan
		//printf("free heap: %d\n",esp_get_free_heap_size());
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}
