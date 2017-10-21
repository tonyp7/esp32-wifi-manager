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
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"

#include "json.h"
#include "http_server.h"
#include "wifi_manager.h"




SemaphoreHandle_t xSemaphoreScan = NULL;
uint16_t ap_num = MAX_AP_NUM;
wifi_ap_record_t *accessp_records; //[MAX_AP_NUM];
char *accessp_json;

EventGroupHandle_t wifi_manager_event_group;
const int wifi_manager_WIFI_CONNECTED_BIT = BIT0;
const int STA_CONNECTED_BIT = BIT1;
const int STA_DISCONNECTED_BIT = BIT2;


wifi_config_t wifi_config;

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
	if(xSemaphoreScan){
		if( xSemaphoreTake( xSemaphoreScan, ( TickType_t ) 10 ) == pdTRUE ) {
			return pdTRUE;
		}
		else{
			return pdFALSE;
		}
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

    case SYSTEM_EVENT_AP_START:
		printf("Access point started -Starting http server\n");
		http_server_set_event_start();
		break;

    case SYSTEM_EVENT_AP_STACONNECTED:
		xEventGroupSetBits(wifi_manager_event_group, STA_CONNECTED_BIT);
		break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
		xEventGroupSetBits(wifi_manager_event_group, STA_DISCONNECTED_BIT);
		break;

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



void wifi_manager_init(){
	wifi_manager_event_group = xEventGroupCreate();

	tcpip_adapter_init();

	ESP_ERROR_CHECK(esp_event_loop_init(wifi_manager_event_handler, NULL));

	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	wifi_config_t wifi_config = {
        .sta = {
            .ssid = "a0308",
            .password = "03080308",
        },
		/*.ap = {
		            .ssid = CONFIG_AP_SSID,
		            .password = CONFIG_AP_PASSWORD,
					.ssid_len = 0,
					.channel = CONFIG_AP_CHANNEL,
					.authmode = CONFIG_AP_AUTHMODE,
					.ssid_hidden = CONFIG_AP_SSID_HIDDEN,
					.max_connection = CONFIG_AP_MAX_CONNECTIONS,
					.beacon_interval = CONFIG_AP_BEACON_INTERVAL,
		},*/
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_manager( void * pvParameters ){

	wifi_manager_event_group = xEventGroupCreate();

	// initialize the tcp stack
	tcpip_adapter_init();

    //event handler
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_manager_event_handler, NULL));



	//try to get access to previously saved wifi
	wifi_sta_config_t* sta_config = get_wifi_sta_config();
	if(sta_config){
		printf("found a saved config\n");
	}
	else{

		//int i = ESP_ERR_WIFI_BASE;

		printf("there is no saved wifi - Starting an access point\n");
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
	    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

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
		printf("Starting access point, SSID=%s\n", ap_config.ap.ssid);

		while(1){
			vTaskDelay(5000 / portTICK_PERIOD_MS);
		}
	}




	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	wifi_config_t wifi_config = {
        .sta = {
            .ssid = "a0308",
            .password = "03080308",
        },
		/*.ap = {
		            .ssid = CONFIG_AP_SSID,
		            .password = CONFIG_AP_PASSWORD,
					.ssid_len = 0,
					.channel = CONFIG_AP_CHANNEL,
					.authmode = CONFIG_AP_AUTHMODE,
					.ssid_hidden = CONFIG_AP_SSID_HIDDEN,
					.max_connection = CONFIG_AP_MAX_CONNECTIONS,
					.beacon_interval = CONFIG_AP_BEACON_INTERVAL,
		},*/
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());


	xEventGroupWaitBits(wifi_manager_event_group, wifi_manager_WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
	//printf("Connected\n\n");

	// print the local IP address
	tcpip_adapter_ip_info_t ip_info;
	ESP_ERROR_CHECK(tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));
	printf("IP Address:  %s\n", ip4addr_ntoa(&ip_info.ip));
	printf("Subnet mask: %s\n", ip4addr_ntoa(&ip_info.netmask));
	printf("Gateway:     %s\n", ip4addr_ntoa(&ip_info.gw));

	// run the mDNS daemon
	mdns_server_t* mDNS = NULL;
	ESP_ERROR_CHECK(mdns_init(TCPIP_ADAPTER_IF_STA, &mDNS));
	ESP_ERROR_CHECK(mdns_set_hostname(mDNS, "esp32"));
	ESP_ERROR_CHECK(mdns_set_instance(mDNS, "Basic HTTP Server"));
	printf("mDNS started\n");


	//wifi scanner config
	wifi_scan_config_t scan_config = {
				.ssid = 0,
				.bssid = 0,
				.channel = 0,
				.show_hidden = true
	};



	//it's now safe to start the HTTP server
	printf("sending event it's ok to start the http server\n");
	http_server_set_event_start();

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
