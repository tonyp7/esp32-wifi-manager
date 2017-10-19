/**
 * \file main.c
 * \author Tony Pottier
 * \brief Entry point for the ESP32 application
 *
 * This software and all resources attached to it are licensed under CC BY 4.0.
 * This software would not be possible to make without these open source softwares:
 *   SpinKit, Copyright 2015 Tobias Ahlin. Licensed under the MIT License.
 *   jQuery, The jQuery Foundation. Licensed under the MIT License.
 *
 * \see https://creativecommons.org/licenses/by-sa/4.0/
 * \see https://idyl.io
 * \see https://github.com/tonyp7/esp32-wifi-manager
 */


#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"



#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/event_groups.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"

#include "http_server.h"
#include "wifi_manager.h"



void monitoring_task(void *pvParameter)
{
	while(1){
		printf("free heap: %d\n",esp_get_free_heap_size());


		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}



EventGroupHandle_t event_group;
const int WIFI_CONNECTED_BIT = BIT0;
// setup and start the wifi connection
// Wifi event handler
esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {

    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;

	case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
        break;

	case SYSTEM_EVENT_STA_DISCONNECTED:
		xEventGroupClearBits(event_group, WIFI_CONNECTED_BIT);
        break;

	default:
        break;
    }
	return ESP_OK;
}

void wifi_setup() {

	event_group = xEventGroupCreate();

	tcpip_adapter_init();

	ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

	wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

	wifi_config_t wifi_config = {
        .sta = {
            .ssid = "a0308",
            .password = "03080308",
        },
    };
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}


char* getAuthModeName(wifi_auth_mode_t auth_mode) {

	char *names[] = {"OPEN", "WEP", "WPA PSK", "WPA2 PSK", "WPA WPA2 PSK", "MAX"};
	return names[auth_mode];
}

void app_main()
{


	// disable the default wifi logging
	esp_log_level_set("wifi", ESP_LOG_NONE);

	//initialize flash memory
	nvs_flash_init();

	//start wifi
	wifi_setup();


	// wait for connection
	//printf("Waiting for connection to the wifi network...\n ");
	xEventGroupWaitBits(event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
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


	wifi_scan_init();


	// start the HTTP Server task
	xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);

	// start the wifi Scanner task
	xTaskCreate(&wifi_manager, "wifi_manager", 2048, NULL, 4, NULL);


	xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);



    //esp_restart();
}
