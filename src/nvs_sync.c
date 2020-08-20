/**
Copyright (c) 2020 Tony Pottier

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

@file nvs_sync.c
@author Tony Pottier
@brief Exposes a simple API to synchronize NVS memory read and writes

@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_err.h>
#include "nvs_sync.h"


static SemaphoreHandle_t nvs_sync_mutex = NULL;

esp_err_t nvs_sync_create(){
    if(nvs_sync_mutex == NULL){

        nvs_sync_mutex = xSemaphoreCreateMutex();

		if(nvs_sync_mutex){
			return ESP_OK;
		}
		else{
			return ESP_FAIL;
		}
    }
	else{
		return ESP_OK;
	}
}

void nvs_sync_free(){
    if(nvs_sync_mutex != NULL){
        vSemaphoreDelete( nvs_sync_mutex );
        nvs_sync_mutex = NULL;
    }
}

bool nvs_sync_lock(TickType_t xTicksToWait){
	if(nvs_sync_mutex){
		if( xSemaphoreTake( nvs_sync_mutex, xTicksToWait ) == pdTRUE ) {
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

void nvs_sync_unlock(){
	xSemaphoreGive( nvs_sync_mutex );
}