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

@file http_server.c
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "mdns.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/opt.h"
#include "lwip/memp.h"
#include "lwip/ip.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/priv/api_msg.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/priv/tcpip_priv.h"

#include "http_server.h"
#include "wifi_manager.h"


EventGroupHandle_t http_server_event_group;
EventBits_t uxBits;

/* embedded binary data */
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
extern const uint8_t jquery_gz_start[] asm("_binary_jquery_gz_start");
extern const uint8_t jquery_gz_end[] asm("_binary_jquery_gz_end");
extern const uint8_t code_js_start[] asm("_binary_code_js_start");
extern const uint8_t code_js_end[] asm("_binary_code_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");


/* const http headers stored in ROM */
const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
const static char http_css_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/css\nCache-Control: public, max-age=31536000\n\n";
const static char http_js_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
const static char http_jquery_gz_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\nAccept-Ranges: bytes\nContent-Length: 29995\nContent-Encoding: gzip\n\n";
const static char http_400_hdr[] = "HTTP/1.1 400 Bad Request\nContent-Length: 0\n\n";
const static char http_404_hdr[] = "HTTP/1.1 404 Not Found\nContent-Length: 0\n\n";
const static char http_503_hdr[] = "HTTP/1.1 503 Service Unavailable\nContent-Length: 0\n\n";
const static char http_ok_json_no_cache_hdr[] = "HTTP/1.1 200 OK\nContent-type: application/json\nCache-Control: no-store, no-cache, must-revalidate, max-age=0\nPragma: no-cache\n\n";


void http_server_set_event_start(){
	xEventGroupSetBits(http_server_event_group, HTTP_SERVER_START_BIT_0 );
}


void http_server(void *pvParameters) {

	http_server_event_group = xEventGroupCreate();

	/* do not start the task until wifi_manager says it's safe to do so! */
#if WIFI_MANAGER_DEBUG
	printf("http_server: waiting for start bit\n");
#endif
	uxBits = xEventGroupWaitBits(http_server_event_group, HTTP_SERVER_START_BIT_0, pdFALSE, pdTRUE, portMAX_DELAY );
#if WIFI_MANAGER_DEBUG
	printf("http_server: received start bit, starting server\n");
#endif

	struct netconn *conn, *newconn;
	err_t err;
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, IP_ADDR_ANY, 80);
	netconn_listen(conn);
	printf("HTTP Server listening...\n");
	do {
		err = netconn_accept(conn, &newconn);
		if (err == ERR_OK) {
			http_server_netconn_serve(newconn);
			netconn_delete(newconn);
		}
		vTaskDelay( (TickType_t)10); /* allows the freeRTOS scheduler to take over if needed */
	} while(err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);
}


char* http_server_get_header(char *request, char *header_name, int *len) {
	*len = 0;
	char *ret = NULL;
	char *ptr = NULL;

	ptr = strstr(request, header_name);
	if (ptr) {
		ret = ptr + strlen(header_name);
		ptr = ret;
		while (*ptr != '\0' && *ptr != '\n' && *ptr != '\r') {
			(*len)++;
			ptr++;
		}
		return ret;
	}
	return NULL;
}


void http_server_netconn_serve(struct netconn *conn) {

	struct netbuf *inbuf;
	char *buf = NULL;
	u16_t buflen;
	err_t err;
	const char new_line[2] = "\n";

	err = netconn_recv(conn, &inbuf);
	if (err == ERR_OK) {

		netbuf_data(inbuf, (void**)&buf, &buflen);

		/* extract the first line of the request */
		char *save_ptr = buf;
		char *line = strtok_r(save_ptr, new_line, &save_ptr);

		if(line) {

			// default page
			if(strstr(line, "GET / ")) {
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, index_html_start, index_html_end - index_html_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /jquery.js ")) {
				netconn_write(conn, http_jquery_gz_hdr, sizeof(http_jquery_gz_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, jquery_gz_start, jquery_gz_end - jquery_gz_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /code.js ")) {
				netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, code_js_start, code_js_end - code_js_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /ap.json ")) {
				/* if we can get the mutex, write the last version of the AP list */
				if(wifi_manager_lock_json_buffer(( TickType_t ) 10)){
					netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY);
					char *buff = wifi_manager_get_ap_list_json();
					netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
					wifi_manager_unlock_json_buffer();
				}
				else{
					netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
#if WIFI_MANAGER_DEBUG
					printf("http_server_netconn_serve: GET /ap.json failed to obtain mutex\n");
#endif
				}
				/* request a wifi scan */
				wifi_manager_scan_async();
			}
			else if(strstr(line, "GET /style.css ")) {
				netconn_write(conn, http_css_hdr, sizeof(http_css_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, style_css_start, style_css_end - style_css_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /status.json ")){
				if(wifi_manager_lock_json_buffer(( TickType_t ) 10)){
					char *buff = wifi_manager_get_ip_info_json();
					if(buff){
						netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY);
						netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
						wifi_manager_unlock_json_buffer();
					}
					else{
						netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
					}
				}
				else{
					netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
#if WIFI_MANAGER_DEBUG
					printf("http_server_netconn_serve: GET /status failed to obtain mutex\n");
#endif
				}
			}
			else if(strstr(line, "DELETE /connect.json ")) {
#if WIFI_MANAGER_DEBUG
				printf("http_server_netconn_serve: DELETE /connect.json\n");
#endif
				/* request a disconnection from wifi and forget about it */
				wifi_manager_disconnect_async();
				netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); /* 200 ok */
			}
			else if(strstr(line, "POST /connect.json ")) {
#if WIFI_MANAGER_DEBUG
				printf("http_server_netconn_serve: POST /connect.json\n");
#endif

				bool found = false;
				int lenS = 0, lenP = 0;
				char *ssid = NULL, *password = NULL;
				ssid = http_server_get_header(save_ptr, "X-Custom-ssid: ", &lenS);
				password = http_server_get_header(save_ptr, "X-Custom-pwd: ", &lenP);

				if(ssid && lenS <= MAX_SSID_SIZE && password && lenP <= MAX_PASSWORD_SIZE){
					wifi_config_t* config = wifi_manager_get_wifi_sta_config();
					memset(config, 0x00, sizeof(wifi_config_t));
					memcpy(config->sta.ssid, ssid, lenS);
					memcpy(config->sta.password, password, lenP);

#if WIFI_MANAGER_DEBUG
					printf("http_server_netconn_serve: wifi_manager_connect_async() call\n");
#endif
					wifi_manager_connect_async();
					netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); //200ok
					found = true;
				}

				if(!found){
					/* bad request the authentification header is not complete/not the correct format */
					netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY);
				}

			}
			else{
				netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY);
			}
		}
		else{
			netconn_write(conn, http_404_hdr, sizeof(http_404_hdr) - 1, NETCONN_NOCOPY);
		}
	}

	/* free the buffer */
	netbuf_delete(inbuf);
}
