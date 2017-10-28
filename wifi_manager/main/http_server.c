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
*/

/**
 * \file http_server.c
 * \author Tony Pottier
 * \brief Defines all functions necessary for the HTTP server to run.
 *
 * Contains the freeRTOS task for the HTTP listener and all necessary support
 * function to process requests, decode URLs, serve files, etc. etc.
 *
 * \note http_server task cannot run without the wifi_manager task!
 * \see https://idyl.io
 * \see https://github.com/tonyp7/esp32-wifi-manager
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

#include "http_server.h"
#include "wifi_manager.h"


EventGroupHandle_t http_server_event_group;
EventBits_t uxBits;

// embedded binary data
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
//extern const uint8_t jquery_js_start[] asm("_binary_jquery_js_start");
//extern const uint8_t jquery_js_end[] asm("_binary_jquery_js_end");
extern const uint8_t jquery_gz_start[] asm("_binary_jquery_gz_start");
extern const uint8_t jquery_gz_end[] asm("_binary_jquery_gz_end");
extern const uint8_t code_js_start[] asm("_binary_code_js_start");
extern const uint8_t code_js_end[] asm("_binary_code_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t lock_png_start[] asm("_binary_lock_png_start");
extern const uint8_t lock_png_end[] asm("_binary_lock_png_end");
extern const uint8_t wifi0_png_start[] asm("_binary_wifi0_png_start");
extern const uint8_t wifi0_png_end[] asm("_binary_wifi0_png_end");
extern const uint8_t wifi1_png_start[] asm("_binary_wifi1_png_start");
extern const uint8_t wifi1_png_end[] asm("_binary_wifi1_png_end");
extern const uint8_t wifi2_png_start[] asm("_binary_wifi2_png_start");
extern const uint8_t wifi2_png_end[] asm("_binary_wifi2_png_end");
extern const uint8_t wifi3_png_start[] asm("_binary_wifi3_png_start");
extern const uint8_t wifi3_png_end[] asm("_binary_wifi3_png_end");


/* const http headers stored in ROM */
const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
const static char http_png_hdr[] = "HTTP/1.1 200 OK\nContent-type: image/png\n\n";
const static char http_css_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/css\n\n";
const static char http_js_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
const static char http_jquery_gz_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\nAccept-Ranges: bytes\nContent-Length: 29995\nContent-Encoding: gzip\n\n";
const static char http_json_hdr[] = "HTTP/1.1 200 OK\nContent-type: application/json\n\n";
const static char http_400_hdr[] = "HTTP/1.1 400 Bad Request\nContent-Length: 0\n\n";
const static char http_404_hdr[] = "HTTP/1.1 404 Not Found\nContent-Length: 0\n\n";
const static char http_503_hdr[] = "HTTP/1.1 503 Service Unavailable\nContent-Length: 0\n\n";





void http_server_set_event_start(){
	xEventGroupSetBits(http_server_event_group, HTTP_SERVER_START_BIT_0 );
}

//I cannot get ctype.h to compile for some weird reason...
int  http_server_isxdigit(int c)
{
    if (c >= '0' && c <= '9') return(true);
    if (c >= 'A' && c <= 'F') return(true);
    if (c >= 'a' && c <= 'f') return(true);
    return(false);
}

void http_server_url_decode(char *dst, const char *src){
    char a, b;
    while (*src) {
            if ((*src == '%') &&
                ((a = src[1]) && (b = src[2])) &&
                ( http_server_isxdigit(a) &&  http_server_isxdigit(b))) {
                    if (a >= 'a')
                            a -= 'a'-'A';
                    if (a >= 'A')
                            a -= ('A' - 10);
                    else
                            a -= '0';
                    if (b >= 'a')
                            b -= 'a'-'A';
                    if (b >= 'A')
                            b -= ('A' - 10);
                    else
                            b -= '0';
                    *dst++ = 16*a+b;
                    src+=3;
            } else if (*src == '+') {
                    *dst++ = ' ';
                    src++;
            } else {
                    *dst++ = *src++;
            }
    }
    *dst++ = '\0';
}

void http_server(void *pvParameters) {

	http_server_event_group = xEventGroupCreate();

	//do not start the task until wifi_manager says it's safe to do so!
	printf("http_server is waiting for notification\n");
	uxBits = xEventGroupWaitBits(http_server_event_group, HTTP_SERVER_START_BIT_0, pdFALSE, pdTRUE, portMAX_DELAY );
	printf("http_server received message to start server\n");

	struct netconn *conn, *newconn;
	err_t err;
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, NULL, 80);
	netconn_listen(conn);
	printf("HTTP Server listening...\n");
	do {
		err = netconn_accept(conn, &newconn);
		//printf("New client connected\n");
		if (err == ERR_OK) {
			http_server_netconn_serve(newconn);
			netconn_delete(newconn);
		}
		vTaskDelay(10); //allow task to be preempted
	} while(err == ERR_OK);
	netconn_close(conn);
	netconn_delete(conn);
}


void http_server_free_parameters(http_parameter* parameters) {
	if (parameters != NULL) {
		int count = sizeof(parameters) / sizeof(http_parameter);
		for (int i = 0; i < count; i++) {
			if(parameters[i].key) free(parameters[i].key);
			if(parameters[i].value) free(parameters[i].value);
			if(parameters[i].raw) free(parameters[i].raw);
		}
		free(parameters);
		parameters = NULL;
	}
}

http_parameter* http_server_decode_parameters(char *body, int *parameter_count) {

	http_parameter* parameters;
	const char param_separator[2] = "&";
	const char keyval_separator[2] = "=";
	int i = 0;
	int buffer_size = 0;
	*parameter_count = 0;


	buffer_size = strlen(body) + 1;
	char *body_cpy = (char*)malloc(sizeof(char) * strlen(body) + 1);
	memset(body_cpy, '\0', sizeof(char) * buffer_size);
	strcpy(body_cpy, body);

	//count number of params
	char *tmp = body_cpy;
	while (*tmp != '\0')
	{
		if (*tmp == '&')
			(*parameter_count)++;
		tmp++;
	}

	if ((*parameter_count)) {

		//there's always 1 more parameter than separators
		(*parameter_count)++;

		//alloc memory for the array
		parameters = (http_parameter*)malloc(sizeof(http_parameter) * (*parameter_count));

		//split the raw components
		tmp = body_cpy;
		char *token = NULL;
		token = strtok(tmp, param_separator);
		while (token != NULL) {

			buffer_size = (strlen(token) + 1);
			char *raw = (char*)malloc(sizeof(char) * buffer_size);
			memset(raw, '\0', sizeof(char) * buffer_size);
			strcpy(raw, token);
			parameters[i].raw = raw;

			//debug
			//printf("raw: %s\n",parameters[i].raw);

			//also safely assign null values in the process!
			parameters[i].key = NULL;
			parameters[i].value = NULL;

			token = strtok(NULL, param_separator);
			i++;
		}

		//we can now safely free the heap
		free(body_cpy);

		//split each raw parameter into key value pairs
		for (i = 0; i<(*parameter_count); i++) {
			char *raw = parameters[i].raw;

			token = strtok(raw, keyval_separator);

			if (token != NULL) {
				//key
				buffer_size = (strlen(token) + 1);
				char *key = (char*)malloc(sizeof(char) * buffer_size);
				memset(key, '\0', sizeof(char) * buffer_size);
				http_server_url_decode(key, token);
				parameters[i].key = key;
				printf("key: %s\n", key);

				//value
				token = strtok(NULL, keyval_separator);
				if (token != NULL) {
					buffer_size = (strlen(token) + 1);
					char *value = (char*)malloc(sizeof(char) * buffer_size);
					memset(value, '\0', sizeof(char) * buffer_size);
					http_server_url_decode(value, token);
					parameters[i].value = value;
					printf("value: %s\n", value);
				}
			}
		}



		return parameters;
	}
	else {
		return NULL;
	}
}

uint8_t http_server_is_valid_connection_parameter(http_parameter* parameters, int count){
	uint8_t found = 0x00;

	for(int i=0; i<count; i++){
		if(strcmp(parameters[i].key, "ssid") == 0){
			found |= 0x01;
		}
		else if(strcmp(parameters[i].key, "password") == 0){
			found |= 0x02;
		}
	}

	return (found == 0x03);
}


void http_server_decode_request(struct netbuf *inbuf, char* first_line, char* body){


	uint8_t first = pdTRUE;
	do {
		char* buf;
		u16_t buflen;

		/* obtain a pointer to the data in the fragment */
		netbuf_data(inbuf, (void**)&buf, &buflen);

		buf[buflen - 1] = '\0';
		printf("%s\n",buf);

		/* process the data */
		for(int i=0;i<buflen;i++){
			if(first && buf[i] == '\n'){
				//first line is found
				memcpy(first_line, buf, i >= 18?19:i+1);
				first = pdFALSE;
			}
		}



	 } while(netbuf_next(inbuf) >= 0);

	netbuf_delete(inbuf);

	//char *ptr = request;
	//while( *ptr != '\0'){
	//	if( *ptr == '\n'){
	//		*ptr = '\0';
	//		return;
	//	}
	//	ptr++;
	//}
}

char* http_server_get_authorization_token(char *buffer, int *len) {
	*len = 0;
	char* ptr = buffer;
	char* ret = NULL;
	while (*ptr != '\0' && *ptr != '\n') {
		if (*ptr == '\x02') {
			ret = ++ptr;
			//find the end of text char
			while (*ptr != '\x03' && *ptr != '\n' && *ptr != '\0') {
				(*len)++;
				ptr++;
			}
			return ret;
		}
		ptr++;
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

		// extract the first line of the request
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
				if(wifi_scan_lock_json_buffer()){
					netconn_write(conn, http_json_hdr, sizeof(http_json_hdr) - 1, NETCONN_NOCOPY);
					char *buff = wifi_manager_get_ap_list_json();
					netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
					wifi_scan_unlock_json_buffer();
				}
				else{
					netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
					printf("Could not get access to json mutex in http_server_netconn_serve GET /ap.json\n");
				}
			}
			else if(strstr(line, "GET /style.css ")) {
				netconn_write(conn, http_css_hdr, sizeof(http_css_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, style_css_start, style_css_end - style_css_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /lock.png ")) {
				netconn_write(conn, http_png_hdr, sizeof(http_png_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, lock_png_start, lock_png_end - lock_png_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /wifi0.png ")) {
				netconn_write(conn, http_png_hdr, sizeof(http_png_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, wifi0_png_start, wifi0_png_end - wifi0_png_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /wifi1.png ")) {
				netconn_write(conn, http_png_hdr, sizeof(http_png_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, wifi1_png_start, wifi1_png_end - wifi1_png_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /wifi2.png ")) {
				netconn_write(conn, http_png_hdr, sizeof(http_png_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, wifi2_png_start, wifi2_png_end - wifi2_png_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /wifi3.png ")) {
				netconn_write(conn, http_png_hdr, sizeof(http_png_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, wifi3_png_start, wifi3_png_end - wifi3_png_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /status ")){
				if(wifi_scan_lock_json_buffer()){
					char *buff = wifi_manager_get_ip_info_json();
					if(buff){
						netconn_write(conn, http_json_hdr, sizeof(http_json_hdr) - 1, NETCONN_NOCOPY);
						netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
						wifi_scan_unlock_json_buffer();
					}
					else{
						netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
					}
				}
				else{
					netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
					printf("Could not get access to json mutex in http_server_netconn_serve GET /status\n");
				}
			}
			else if(strstr(line, "POST /connect ")) {
				//printf("%s\n\n",buf);
				uint8_t found = pdFALSE;
				while((line = strtok_r(save_ptr, new_line, &save_ptr))){
					if(strstr(line, "Authorization:")){
						int lenS = 0, lenP = 0;
						char *ssid = http_server_get_authorization_token(line, &lenS);
						char *password = NULL;
						if(ssid && lenS <= MAX_SSID_SIZE){
							password = http_server_get_authorization_token(ssid, &lenP);
							if(password && lenP <= MAX_PASSWORD_SIZE){
								wifi_config_t* config = wifi_manager_get_wifi_sta_config();
								//wifi_config_t* config = (wifi_config_t*)malloc(sizeof(wifi_config_t));
								memset(config, 0x00, sizeof(wifi_config_t));
								memcpy(config->sta.ssid, ssid, lenS);
								memcpy(config->sta.password, password, lenP);
								//printf("ssid: %s\n", config->sta.ssid);
								//printf("pwd: %s\n", config->sta.password);

								//initialize connection sequence
								wifi_manager_connect_async();
								netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY); //200ok
								found = pdTRUE;
							}
							else{
								break; /* no point going further, this request is bad and won't be processed */
							}
						}
						else{
							break; /* no point going further, this request is bad and won't be processed */
						}
						break; /* found the Authorization header, no point going further */
					}
				}

				if(!found){
					//bad request
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

	// close the connection and free the buffer
	netbuf_delete(inbuf);
	netconn_close(conn);

}
