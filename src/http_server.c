/*
Copyright (c) 2017-2019 Tony Pottier

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
#include "esp_event.h"
#include "esp_log.h"
#include "lwip/netbuf.h"
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

#include "../../main/includes/ruuvidongle.h"
#include "../../main/includes/ethernet.h"
#include "cJSON.h"

#define FULLBUF_SIZE 4*1024

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
/* @brief tag used for ESP serial console messages */
static const char TAG[] = "http_server";

/* @brief task handle for the http server */
static TaskHandle_t task_http_server = NULL;


/**
 * @brief embedded binary data.
 * @see file "component.mk"
 * @see https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#embedding-binary-data
 */
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
extern const uint8_t jquery_gz_start[] asm("_binary_jquery_gz_start");
extern const uint8_t jquery_gz_end[] asm("_binary_jquery_gz_end");
extern const uint8_t code_js_start[] asm("_binary_code_js_start");
extern const uint8_t code_js_end[] asm("_binary_code_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t ruuvi_html_start[] asm("_binary_ruuvi_html_start");
extern const uint8_t ruuvi_html_end[] asm("_binary_ruuvi_html_end");
extern const uint8_t ruuvi_js_start[] asm("_binary_ruuvi_js_start");
extern const uint8_t ruuvi_js_end[] asm("_binary_ruuvi_js_end");


/* const http headers stored in ROM */
const static char http_html_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/html\n\n";
const static char http_css_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/css\nCache-Control: public, max-age=31536000\n\n";
const static char http_js_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\n\n";
const static char http_jquery_gz_hdr[] = "HTTP/1.1 200 OK\nContent-type: text/javascript\nAccept-Ranges: bytes\nContent-Length: 29995\nContent-Encoding: gzip\n\n";
const static char http_400_hdr[] = "HTTP/1.1 400 Bad Request\nContent-Length: 0\n\n";
const static char http_404_hdr[] = "HTTP/1.1 404 Not Found\nContent-Length: 0\n\n";
const static char http_503_hdr[] = "HTTP/1.1 503 Service Unavailable\nContent-Length: 0\n\n";
const static char http_ok_json_no_cache_hdr[] = "HTTP/1.1 200 OK\nContent-type: application/json\nCache-Control: no-store, no-cache, must-revalidate, max-age=0\nPragma: no-cache\n\n";
const static char http_redirect_hdr_start[] = "HTTP/1.1 302 Found\nLocation: http://";
const static char http_redirect_hdr_end[] = "/\n\n";



void http_server_start(){
	esp_log_level_set(TAG, ESP_LOG_DEBUG);
	if(task_http_server == NULL){
		xTaskCreate(&http_server, "http_server", 20*1024, NULL, WIFI_MANAGER_TASK_PRIORITY-1, &task_http_server);
	}
}

void http_server_stop() {
	if (task_http_server) {
		vTaskDelete(task_http_server);
		task_http_server = NULL;
	}
}

void http_server(void *pvParameters) {

	struct netconn *conn, *newconn;
	err_t err;
	conn = netconn_new(NETCONN_TCP);
	netconn_bind(conn, IP_ADDR_ANY, 80);
	netconn_listen(conn);
	ESP_LOGI(TAG, "HTTP Server listening on 80/tcp");
	for(;;) {
		err = netconn_accept(conn, &newconn);
		if (err == ERR_OK) {
			http_server_netconn_serve(newconn);
			netconn_close(newconn);
			netconn_delete(newconn);
		}
		else if(err == ERR_TIMEOUT){
			ESP_LOGE(TAG, "http_server: netconn_accept ERR_TIMEOUT");
		}
		else if(err == ERR_ABRT){
			ESP_LOGE(TAG, "http_server: netconn_accept ERR_ABRT");
		}
		else {
			ESP_LOGE(TAG, "http_server: netconn_accept: %d", err);
		}
		taskYIELD();  /* allows the freeRTOS scheduler to take over if needed. */
	}
	netconn_close(conn);
	netconn_delete(conn);

	vTaskDelete( NULL );
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

char* get_http_body(char* msg, int len, int* blen)
{
	char* newlines = "\r\n\r\n";
	char* p = strstr(msg, newlines);
	if (p) {
		p += strlen(newlines);
	} else {
		ESP_LOGD(TAG, "http body not found: %s", msg);
		return 0;
	}

	if (blen) {
		*blen = len - (p - msg);
	}

	return p;
}

bool parse_ruuvi_config_json(const char* body, struct dongle_config *c)
{
	//TODO replace this parsing with generic implementation
	bool ret = true;
	cJSON* root = cJSON_Parse(body);
	if (root) {
		ESP_LOGD(TAG, "settings parsed from posted json:");

		cJSON* ed = cJSON_GetObjectItem(root, "eth_dhcp");
		if (ed) {
			bool eth_dhcp = cJSON_IsTrue(ed);
			c->eth_dhcp = eth_dhcp;
			ESP_LOGD(TAG, "eth_dhcp: %d", eth_dhcp);
		}

		cJSON* esip = cJSON_GetObjectItem(root, "eth_static_ip");
		if (esip) {
			char* eth_static_ip = cJSON_GetStringValue(esip);
			if (eth_static_ip) {
				strncpy(c->eth_static_ip, eth_static_ip, IP_STR_LEN);
				ESP_LOGD(TAG, "eth_static_ip: %s", eth_static_ip);
			}
		}

		cJSON* enm = cJSON_GetObjectItem(root, "eth_netmask");
		if (enm) {
			char* eth_netmask = cJSON_GetStringValue(enm);
			if (eth_netmask) {
				strncpy(c->eth_netmask, eth_netmask, IP_STR_LEN);
				ESP_LOGD(TAG, "eth_netmask: %s", eth_netmask);
			}
		}

		cJSON* egw = cJSON_GetObjectItem(root, "eth_gw");
		if (egw) {
			char* eth_gw = cJSON_GetStringValue(egw);
			if (eth_gw) {
				strncpy(c->eth_gw, eth_gw, IP_STR_LEN);
				ESP_LOGD(TAG, "eth_gw: %s", eth_gw);
			}
		}

		cJSON* edns1 = cJSON_GetObjectItem(root, "eth_dns1");
		if (edns1) {
			char* eth_dns1 = cJSON_GetStringValue(edns1);
			if (eth_dns1) {
				strncpy(c->eth_dns1, eth_dns1, IP_STR_LEN);
				ESP_LOGD(TAG, "eth_dns1: %s", eth_dns1);
			}
		}

		cJSON* edns2 = cJSON_GetObjectItem(root, "eth_dns2");
		if (edns2) {
			char* eth_dns2 = cJSON_GetStringValue(edns2);
			if (eth_dns2) {
				strncpy(c->eth_dns2, eth_dns2, IP_STR_LEN);
				ESP_LOGD(TAG, "eth_dns2: %s", eth_dns2);
			}
		}

		cJSON* um = cJSON_GetObjectItem(root, "use_mqtt");
		if (um) {
			bool use_mqtt = cJSON_IsTrue(um);
			c->use_mqtt = use_mqtt;
			ESP_LOGD(TAG, "use_mqtt: %d", use_mqtt);
		} else {
			ESP_LOGE(TAG, "use_mqtt not found");
		}

		cJSON* ms = cJSON_GetObjectItem(root, "mqtt_server");
		if (ms) {
			char* mqtt_server = cJSON_GetStringValue(ms);
			if (mqtt_server) {
				strncpy(c->mqtt_server, mqtt_server, MAX_MQTTUSER_LEN-1);
				ESP_LOGD(TAG, "mqtt_server: %s", mqtt_server);
			}
		}

		cJSON* mpre = cJSON_GetObjectItem(root, "mqtt_prefix");
		if (mpre) {
			char* mqtt_prefix = cJSON_GetStringValue(mpre);
			if (mqtt_prefix) {
				strncpy(c->mqtt_prefix, mqtt_prefix, MAX_MQTTPREFIX_LEN-1);
				ESP_LOGD(TAG, "mqtt_prefix: %s", mqtt_prefix);
			}
		}

		cJSON* mpo = cJSON_GetObjectItem(root, "mqtt_port");
		if (cJSON_IsNumber(mpo)) {
			uint32_t mqtt_port = mpo->valueint;
			if (mqtt_port) {
				c->mqtt_port = mqtt_port;
				ESP_LOGD(TAG, "mqtt_port: %d", mqtt_port);
			}
		} else {
			ESP_LOGE(TAG, "mqtt port not found");
		}

		cJSON* mu = cJSON_GetObjectItem(root, "mqtt_user");
		if (mu) {
			char* mqtt_user = cJSON_GetStringValue(mu);
			if (mqtt_user) {
				strncpy(c->mqtt_user, mqtt_user, MAX_MQTTUSER_LEN-1);
				ESP_LOGD(TAG, "mqtt_user: %s", mqtt_user);
			}
		} else {
			ESP_LOGE(TAG, "mqtt_user not found");
		}

		cJSON* mp = cJSON_GetObjectItem(root, "mqtt_pass");
		if (mp) {
			char* mqtt_pass = cJSON_GetStringValue(mp);
			if (mqtt_pass) {
				strncpy(c->mqtt_pass, mqtt_pass, MAX_MQTTPASS_LEN-1);
				ESP_LOGD(TAG, "mqtt_pass: %s", mqtt_pass);
			}
		} else {
			ESP_LOGW(TAG, "mqtt_pass not found or not changed");
		}

		cJSON* uh = cJSON_GetObjectItem(root, "use_http");
		if (uh) {
			bool use_http = cJSON_IsTrue(uh);
			c->use_http = use_http;
			ESP_LOGD(TAG, "use_http: %d", use_http);
		} else {
			ESP_LOGE(TAG, "use_http not found");
		}

		cJSON* hurl = cJSON_GetObjectItem(root, "http_url");
		if (hurl) {
			char* http_url = cJSON_GetStringValue(hurl);
			if (http_url) {
				strncpy(c->http_url, http_url, MAX_HTTPURL_LEN-1);
				ESP_LOGD(TAG, "http_url: %s", http_url);
			}
		} else {
			ESP_LOGE(TAG, "http_url not found");
		}

		cJSON* uf = cJSON_GetObjectItem(root, "use_filtering");
		if (uf) {
			bool use_filtering = cJSON_IsTrue(uf);
			c->company_filter = use_filtering;
			ESP_LOGD(TAG, "use_filtering: %d", use_filtering);
		} else {
			ESP_LOGE(TAG, "use_filtering not found");
		}

		cJSON* cid = cJSON_GetObjectItem(root, "company_id");
		if (cid) {
			char* company_id = cJSON_GetStringValue(cid);
			if (company_id) {
				uint16_t c_id = (uint16_t)strtol(company_id, NULL, 0);
				ESP_LOGD(TAG, "company_id: 0x%02x", c_id);
				c->company_id = c_id;
			} else {
				ESP_LOGE(TAG, "company id not found");
			}
		}

		cJSON* co = cJSON_GetObjectItem(root, "coordinates");
		if (co) {
			char* coordinates = cJSON_GetStringValue(co);
			if (coordinates) {
				strncpy(c->coordinates, coordinates, MAX_CONFIG_STR_LEN-1);
				ESP_LOGD(TAG, "coordinates: %s", coordinates);
			}
		} else {
			ESP_LOGE(TAG, "coordinates not found");
		}
	} else {
		ESP_LOGE(TAG, "Can't parse json: %s", body);
		ret = false;
	}

	cJSON_Delete(root);
	return ret;
}

void http_server_netconn_serve(struct netconn *conn) {
	struct netbuf *inbuf;
	char *buf = NULL;
	char fullbuf[FULLBUF_SIZE+1];
	uint fullsize = 0;
	u16_t buflen;
	err_t err;
	const char new_line[2] = "\n";
	bool request_ready = false;

	while(request_ready == false) {
		err = netconn_recv(conn, &inbuf);
		if (err == ERR_OK) {
			netbuf_data(inbuf, (void**)&buf, &buflen);

			if (fullsize + buflen > FULLBUF_SIZE) {
				ESP_LOGW(TAG, "fullbuf full, fullsize: %d, buflen: %d", fullsize, buflen);
				netbuf_delete(inbuf);
				break;
			} else {
				memcpy(fullbuf + fullsize, buf, buflen);
				fullsize += buflen;
				fullbuf[fullsize] = 0; //zero terminated string
			}

			netbuf_delete(inbuf);

			//check if there should be more data coming from conn
			int hLen = 0;
			char* cl = http_server_get_header(fullbuf, "Content-Length: ", &hLen);
			if (cl) {
				int body_len;
				int clen =  atoi(cl);
				char* b = get_http_body(fullbuf, fullsize, &body_len);
				if (b) {
					ESP_LOGD(TAG, "Header Content-Length: %d, HTTP body length: %d", clen, body_len);
					if (clen == body_len) {
						//HTTP request is full
						request_ready = true;
					} else {
						ESP_LOGD(TAG, "request not full yet");
					}
				} else {
					ESP_LOGD(TAG, "Header Content-Length: %d, body not found", clen);
					//read more data
				}
			} else {
				//ESP_LOGD(TAG, "no Content-Length header");
				request_ready = true;
			}
		} else {
			ESP_LOGW(TAG, "netconn recv: %d", err);
			break;
		}
	}

	if (!request_ready)
	{
		ESP_LOGW(TAG, "the connection was closed by the client side");
		return;
	}

	buf = fullbuf;
	char *save_ptr = fullbuf;
	buflen = fullsize;

	ESP_LOGD(TAG, "req: %s", buf);

	char *line = strtok_r(save_ptr, new_line, &save_ptr);

	if(line) {
		/* captive portal functionality: redirect to access point IP for HOST that are not the access point IP OR the STA IP */
		int lenH = 0;
		char *host = http_server_get_header(save_ptr, "Host: ", &lenH);
		/* determine if Host is from the STA IP address */
		wifi_manager_lock_sta_ip_string(portMAX_DELAY);
		bool access_from_sta_ip = lenH > 0?strstr(host, wifi_manager_get_sta_ip_string()):false;
		wifi_manager_unlock_sta_ip_string();

		if (lenH > 0 && !strstr(host, DEFAULT_AP_IP) && !access_from_sta_ip) {
			netconn_write(conn, http_redirect_hdr_start, sizeof(http_redirect_hdr_start) - 1, NETCONN_NOCOPY);
			netconn_write(conn, DEFAULT_AP_IP, sizeof(DEFAULT_AP_IP) - 1, NETCONN_NOCOPY);
			netconn_write(conn, http_redirect_hdr_end, sizeof(http_redirect_hdr_end) - 1, NETCONN_NOCOPY);
		}
		else{
			/* default page */
			if(strstr(line, "GET / ")) {
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, index_html_start, index_html_end - index_html_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /ruuvi.html ")) {
				netconn_write(conn, http_html_hdr, sizeof(http_html_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, ruuvi_html_start, ruuvi_html_end - ruuvi_html_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /ruuvi.js ")) {
				netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, ruuvi_js_start, ruuvi_js_end - ruuvi_js_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /jquery.js ")) {
				netconn_write(conn, http_jquery_gz_hdr, sizeof(http_jquery_gz_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, jquery_gz_start, jquery_gz_end - jquery_gz_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /code.js ")) {
				netconn_write(conn, http_js_hdr, sizeof(http_js_hdr) - 1, NETCONN_NOCOPY);
				netconn_write(conn, code_js_start, code_js_end - code_js_start, NETCONN_NOCOPY);
			}
			else if(strstr(line, "GET /ruuvi.json ")) {
				ESP_LOGI(TAG, "GET /ruuvi.json");
				netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY);
				char* ruuvi_json = ruuvi_get_conf_json();
				ESP_LOGD(TAG, "configuration json to browser: %s", ruuvi_json);
				netconn_write(conn, ruuvi_json, strlen(ruuvi_json), NETCONN_COPY);
				free(ruuvi_json);
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
					ESP_LOGD(TAG, "http_server_netconn_serve: GET /ap.json failed to obtain mutex");
				}
				/* request a wifi scan */
				//wifi_manager_scan_async();
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
					ESP_LOGD(TAG, "http_server_netconn_serve: GET /status failed to obtain mutex");
				}
			}
			else if(strstr(line, "DELETE /connect.json ")) {
				ESP_LOGD(TAG, "http_server_netconn_serve: DELETE /connect.json");
				/* request a disconnection from wifi and forget about it */
				wifi_manager_disconnect_async();
				netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); /* 200 ok */
			}
			else if(strstr(line, "POST /connect.json ")) {
				ESP_LOGD(TAG, "http_server_netconn_serve: POST /connect.json");

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
					ESP_LOGD(TAG, "http_server_netconn_serve: wifi_manager_connect_async() call");
					wifi_manager_connect_async();
					netconn_write(conn, http_ok_json_no_cache_hdr, sizeof(http_ok_json_no_cache_hdr) - 1, NETCONN_NOCOPY); //200ok
					found = true;
				}

				if(!found){
					/* bad request the authentification header is not complete/not the correct format */
					netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY);
				}

			} else if(strstr(line, "POST /ruuvi.json ")) {
				ESP_LOGD(TAG, "http_server_netconn_serve: POST /ruuvi.json");
				//ESP_LOGI(TAG, "POST: %s", save_ptr);
				//char* body = get_http_body(save_ptr, buflen - (save_ptr - buf));

				char* body = get_http_body(save_ptr, buflen - (save_ptr - buf), 0);
				if (parse_ruuvi_config_json(body, &m_dongle_config)) {
					ESP_LOGI(TAG, "settings got from browser:");
					settings_print(&m_dongle_config);
					settings_save_to_flash(&m_dongle_config);
					ruuvi_send_nrf_settings(&m_dongle_config);
					ethernet_update_ip();
				}
			} else{
				netconn_write(conn, http_400_hdr, sizeof(http_400_hdr) - 1, NETCONN_NOCOPY);
			}
		}
	}
	else{
		netconn_write(conn, http_404_hdr, sizeof(http_404_hdr) - 1, NETCONN_NOCOPY);
	}
}
