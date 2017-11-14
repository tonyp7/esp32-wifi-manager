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

@file main.c
@author Tony Pottier
@brief Entry point for the ESP32 application.
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
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



static TaskHandle_t task_http_server = NULL;
static TaskHandle_t task_wifi_manager = NULL;

/**
 * @brief RTOS task that periodically prints the heap memory available.
 * @note Pure debug information, should not be ever started on production code!
 */
void monitoring_task(void *pvParameter)
{
	for(;;){
		printf("free heap: %d\n",esp_get_free_heap_size());
		vTaskDelay(5000 / portTICK_PERIOD_MS);
	}
}


void app_main()
{


	/* disable the default wifi logging */
	esp_log_level_set("wifi", ESP_LOG_NONE);

	/* initialize flash memory */
	nvs_flash_init();

	/* start the HTTP Server task */
	xTaskCreate(&http_server, "http_server", 2048, NULL, 5, &task_http_server);

	/* start the wifi manager task */
	xTaskCreate(&wifi_manager, "wifi_manager", 4096, NULL, 4, &task_wifi_manager);

	/* your code should go here. In debug mode we create a simple task on core 2 that monitors free heap memory */
#if WIFI_MANAGER_DEBUG
	xTaskCreatePinnedToCore(&monitoring_task, "monitoring_task", 2048, NULL, 1, NULL, 1);
#endif


}
