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


// HTTP headers and web pages
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


void http_server_netconn_serve(struct netconn *conn) {

	struct netbuf *inbuf;
	char *buf;
	u16_t buflen;
	err_t err;
	const char new_line[2] = "\n";
	char data[500];

	err = netconn_recv(conn, &inbuf);

	if (err == ERR_OK) {


		//netbuf_data(inbuf, (void**)&buf, &buflen);
		netbuf_copy(inbuf, data, 500);
		buf = data;
		//printf("%s\n",buf);

		// extract the first line, with the request
		char *line = strtok(buf, new_line);

		if(line) {

			// default page
			if(strstr(line, "GET / ")) {
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, index_html_start, index_html_end - index_html_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /jquery.js ")) {
				/*Accept-Ranges: bytes
				Content-Length: 438
				Connection: close
				Content-Type: text/html; charset=UTF-8
				Content-Encoding: gzip*/
				//netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
				//netconn_write(conn, jquery_js_start, jquery_js_end - jquery_js_start, NETCONN_NOCOPY);
				netconn_write(conn, http_jquery_gz_hdr, sizeof(http_jquery_gz_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, jquery_gz_start, jquery_gz_end - jquery_gz_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /code.js ")) {
				netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, code_js_start, code_js_end - code_js_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /ap.json ")) {
				//printf("GET /ap.json\n");
				if(wifi_scan_lock_ap_list()){
					netconn_write(conn, http_json_hdr, sizeof(http_json_hdr) - 1, NETCONN_NOCOPY);
					char *buff = wifi_scan_get_json();
					netconn_write(conn, buff, strlen(buff), NETCONN_NOCOPY);
					wifi_scan_unlock_ap_list();
				}
				else{
					netconn_write(conn, http_503_hdr, sizeof(http_503_hdr) - 1, NETCONN_NOCOPY);
					printf("Could not get access to xSemaphoreScan in http_server_netconn_serve\n");
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
			else if(strstr(line, "POST /connect ")) {
				printf("POST /connect\n");
				//ignore all http headers
				while( line != NULL ) {
					line = strtok(NULL, new_line);

					if(line != NULL && (strlen(line) == 0 || (strlen(line) == 1 && line[0] == '\r') )){
						//empty line separating the HTTP headers from the body
						line = strtok(NULL, new_line);
						break;
					}
				}

				//get the body of the request
				if(line != NULL){
					printf("BODY: %s\n",line);
					int len = strlen(line);
					for(int i=0;i<len;i++){
						printf("%04x ", line[i]);
					}
					printf("\n");
					int count = 0;
					http_parameter* parameters = http_server_decode_parameters(line, &count);
					if(http_server_is_valid_connection_parameter(parameters, count)){
						printf("found valid request\n");
						http_server_free_parameters(parameters);
					}
					else{
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
		else printf("Invalid HTTP request ignored\n");
	}

	// close the connection and free the buffer
	netconn_close(conn);
	netbuf_delete(inbuf);
}
