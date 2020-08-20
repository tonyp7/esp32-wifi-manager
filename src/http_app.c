/*
Copyright (c) 2017-2020 Tony Pottier

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

@file http_app.c
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
#include <stdint.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include "esp_netif.h"
#include <esp_http_server.h>

#include "wifi_manager.h"
#include "http_app.h"


/* @brief tag used for ESP serial console messages */
static const char TAG[] = "http_server";

/* @brief the HTTP server handle */
static httpd_handle_t httpd_handle = NULL;

/* function pointers to URI handlers that can be user made */
esp_err_t (*custom_get_httpd_uri_handler)(httpd_req_t *r) = NULL;
esp_err_t (*custom_post_httpd_uri_handler)(httpd_req_t *r) = NULL;

/* strings holding the URLs of the wifi manager */
static char* http_root_url = NULL;
static char* http_redirect_url = NULL;
static char* http_js_url = NULL;
static char* http_css_url = NULL;
static char* http_connect_url = NULL;
static char* http_ap_url = NULL;
static char* http_status_url = NULL;

/**
 * @brief embedded binary data.
 * @see file "component.mk"
 * @see https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#embedding-binary-data
 */
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[]   asm("_binary_style_css_end");
extern const uint8_t code_js_start[] asm("_binary_code_js_start");
extern const uint8_t code_js_end[] asm("_binary_code_js_end");
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");


/* const httpd related values stored in ROM */
const static char http_200_hdr[] = "200 OK";
const static char http_302_hdr[] = "302 Found";
const static char http_400_hdr[] = "400 Bad Request";
const static char http_404_hdr[] = "404 Not Found";
const static char http_503_hdr[] = "503 Service Unavailable";
const static char http_location_hdr[] = "Location";
const static char http_content_type_html[] = "text/html";
const static char http_content_type_js[] = "text/javascript";
const static char http_content_type_css[] = "text/css";
const static char http_content_type_json[] = "application/json";
const static char http_cache_control_hdr[] = "Cache-Control";
const static char http_cache_control_no_cache[] = "no-store, no-cache, must-revalidate, max-age=0";
const static char http_cache_control_cache[] = "public, max-age=31536000";
const static char http_pragma_hdr[] = "Pragma";
const static char http_pragma_no_cache[] = "no-cache";



esp_err_t http_app_set_handler_hook( httpd_method_t method,  esp_err_t (*handler)(httpd_req_t *r)  ){

	if(method == HTTP_GET){
		custom_get_httpd_uri_handler = handler;
		return ESP_OK;
	}
	else if(method == HTTP_POST){
		custom_post_httpd_uri_handler = handler;
		return ESP_OK;
	}
	else{
		return ESP_ERR_INVALID_ARG;
	}

}


static esp_err_t http_server_delete_handler(httpd_req_t *req){

	ESP_LOGI(TAG, "DELETE %s", req->uri);

	/* DELETE /connect.json */
	if(strcmp(req->uri, http_connect_url) == 0){
		wifi_manager_disconnect_async();

		httpd_resp_set_status(req, http_200_hdr);
		httpd_resp_set_type(req, http_content_type_json);
		httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_no_cache);
		httpd_resp_set_hdr(req, http_pragma_hdr, http_pragma_no_cache);
		httpd_resp_send(req, NULL, 0);
	}
	else{
		httpd_resp_set_status(req, http_404_hdr);
		httpd_resp_send(req, NULL, 0);
	}

	return ESP_OK;
}


static esp_err_t http_server_post_handler(httpd_req_t *req){


	esp_err_t ret = ESP_OK;

	ESP_LOGI(TAG, "POST %s", req->uri);

	/* POST /connect.json */
	if(strcmp(req->uri, http_connect_url) == 0){


		/* buffers for the headers */
		size_t ssid_len = 0, password_len = 0;
		char *ssid = NULL, *password = NULL;

		/* len of values provided */
		ssid_len = httpd_req_get_hdr_value_len(req, "X-Custom-ssid");
		password_len = httpd_req_get_hdr_value_len(req, "X-Custom-pwd");


		if(ssid_len && ssid_len <= MAX_SSID_SIZE && password_len && password_len <= MAX_PASSWORD_SIZE){

			/* get the actual value of the headers */
			ssid = malloc(sizeof(char) * (ssid_len + 1));
			password = malloc(sizeof(char) * (password_len + 1));
			httpd_req_get_hdr_value_str(req, "X-Custom-ssid", ssid, ssid_len+1);
			httpd_req_get_hdr_value_str(req, "X-Custom-pwd", password, password_len+1);

			wifi_config_t* config = wifi_manager_get_wifi_sta_config();
			memset(config, 0x00, sizeof(wifi_config_t));
			memcpy(config->sta.ssid, ssid, ssid_len);
			memcpy(config->sta.password, password, password_len);
			ESP_LOGI(TAG, "ssid: %s, password: %s", ssid, password);
			ESP_LOGD(TAG, "http_server_post_handler: wifi_manager_connect_async() call");
			wifi_manager_connect_async();

			/* free memory */
			free(ssid);
			free(password);

			httpd_resp_set_status(req, http_200_hdr);
			httpd_resp_set_type(req, http_content_type_json);
			httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_no_cache);
			httpd_resp_set_hdr(req, http_pragma_hdr, http_pragma_no_cache);
			httpd_resp_send(req, NULL, 0);

		}
		else{
			/* bad request the authentification header is not complete/not the correct format */
			httpd_resp_set_status(req, http_400_hdr);
			httpd_resp_send(req, NULL, 0);
		}

	}
	else{

		if(custom_post_httpd_uri_handler == NULL){
			httpd_resp_set_status(req, http_404_hdr);
			httpd_resp_send(req, NULL, 0);
		}
		else{

			/* if there's a hook, run it */
			ret = (*custom_post_httpd_uri_handler)(req);
		}
	}

	return ret;
}


static esp_err_t http_server_get_handler(httpd_req_t *req){

    char* host = NULL;
    size_t buf_len;
    esp_err_t ret = ESP_OK;

    ESP_LOGD(TAG, "GET %s", req->uri);

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
    	host = malloc(buf_len);
    	if(httpd_req_get_hdr_value_str(req, "Host", host, buf_len) != ESP_OK){
    		/* if something is wrong we just 0 the whole memory */
    		memset(host, 0x00, buf_len);
    	}
    }

	/* determine if Host is from the STA IP address */
	wifi_manager_lock_sta_ip_string(portMAX_DELAY);
	bool access_from_sta_ip = host != NULL?strstr(host, wifi_manager_get_sta_ip_string()):false;
	wifi_manager_unlock_sta_ip_string();


	if (host != NULL && !strstr(host, DEFAULT_AP_IP) && !access_from_sta_ip) {

		/* Captive Portal functionality */
		/* 302 Redirect to IP of the access point */
		httpd_resp_set_status(req, http_302_hdr);
		httpd_resp_set_hdr(req, http_location_hdr, http_redirect_url);
		httpd_resp_send(req, NULL, 0);

	}
	else{

		/* GET /  */
		if(strcmp(req->uri, http_root_url) == 0){
			httpd_resp_set_status(req, http_200_hdr);
			httpd_resp_set_type(req, http_content_type_html);
			httpd_resp_send(req, (char*)index_html_start, index_html_end - index_html_start);
		}
		/* GET /code.js */
		else if(strcmp(req->uri, http_js_url) == 0){
			httpd_resp_set_status(req, http_200_hdr);
			httpd_resp_set_type(req, http_content_type_js);
			httpd_resp_send(req, (char*)code_js_start, code_js_end - code_js_start);
		}
		/* GET /style.css */
		else if(strcmp(req->uri, http_css_url) == 0){
			httpd_resp_set_status(req, http_200_hdr);
			httpd_resp_set_type(req, http_content_type_css);
			httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_cache);
			httpd_resp_send(req, (char*)style_css_start, style_css_end - style_css_start);
		}
		/* GET /ap.json */
		else if(strcmp(req->uri, http_ap_url) == 0){

			/* if we can get the mutex, write the last version of the AP list */
			if(wifi_manager_lock_json_buffer(( TickType_t ) 10)){

				httpd_resp_set_status(req, http_200_hdr);
				httpd_resp_set_type(req, http_content_type_json);
				httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_no_cache);
				httpd_resp_set_hdr(req, http_pragma_hdr, http_pragma_no_cache);
				char* ap_buf = wifi_manager_get_ap_list_json();
				httpd_resp_send(req, ap_buf, strlen(ap_buf));
				wifi_manager_unlock_json_buffer();
			}
			else{
				httpd_resp_set_status(req, http_503_hdr);
				httpd_resp_send(req, NULL, 0);
				ESP_LOGE(TAG, "http_server_netconn_serve: GET /ap.json failed to obtain mutex");
			}

			/* request a wifi scan */
			wifi_manager_scan_async();
		}
		/* GET /status.json */
		else if(strcmp(req->uri, http_status_url) == 0){

			if(wifi_manager_lock_json_buffer(( TickType_t ) 10)){
				char *buff = wifi_manager_get_ip_info_json();
				if(buff){
					httpd_resp_set_status(req, http_200_hdr);
					httpd_resp_set_type(req, http_content_type_json);
					httpd_resp_set_hdr(req, http_cache_control_hdr, http_cache_control_no_cache);
					httpd_resp_set_hdr(req, http_pragma_hdr, http_pragma_no_cache);
					httpd_resp_send(req, buff, strlen(buff));
					wifi_manager_unlock_json_buffer();
				}
				else{
					httpd_resp_set_status(req, http_503_hdr);
					httpd_resp_send(req, NULL, 0);
				}
			}
			else{
				httpd_resp_set_status(req, http_503_hdr);
				httpd_resp_send(req, NULL, 0);
				ESP_LOGE(TAG, "http_server_netconn_serve: GET /status.json failed to obtain mutex");
			}
		}
		else{

			if(custom_get_httpd_uri_handler == NULL){
				httpd_resp_set_status(req, http_404_hdr);
				httpd_resp_send(req, NULL, 0);
			}
			else{

				/* if there's a hook, run it */
				ret = (*custom_get_httpd_uri_handler)(req);
			}
		}

	}

    /* memory clean up */
    if(host != NULL){
    	free(host);
    }

    return ret;

}

/* URI wild card for any GET request */
static const httpd_uri_t http_server_get_request = {
    .uri       = "*",
    .method    = HTTP_GET,
    .handler   = http_server_get_handler
};

static const httpd_uri_t http_server_post_request = {
	.uri	= "*",
	.method = HTTP_POST,
	.handler = http_server_post_handler
};

static const httpd_uri_t http_server_delete_request = {
	.uri	= "*",
	.method = HTTP_DELETE,
	.handler = http_server_delete_handler
};


void http_app_stop(){

	if(httpd_handle != NULL){


		/* dealloc URLs */
		if(http_root_url) {
			free(http_root_url);
			http_root_url = NULL;
		}
		if(http_redirect_url){
			free(http_redirect_url);
			http_redirect_url = NULL;
		}
		if(http_js_url){
			free(http_js_url);
			http_js_url = NULL;
		}
		if(http_css_url){
			free(http_css_url);
			http_css_url = NULL;
		}
		if(http_connect_url){
			free(http_connect_url);
			http_connect_url = NULL;
		}
		if(http_ap_url){
			free(http_ap_url);
			http_ap_url = NULL;
		}
		if(http_status_url){
			free(http_status_url);
			http_status_url = NULL;
		}

		/* stop server */
		httpd_stop(httpd_handle);
		httpd_handle = NULL;
	}
}


/**
 * @brief helper to generate URLs of the wifi manager
 */
static char* http_app_generate_url(const char* page){

	char* ret;

	int root_len = strlen(WEBAPP_LOCATION);
	const size_t url_sz = sizeof(char) * ( (root_len+1) + ( strlen(page) + 1) );

	ret = malloc(url_sz);
	memset(ret, 0x00, url_sz);
	strcpy(ret, WEBAPP_LOCATION);
	ret = strcat(ret, page);

	return ret;
}

void http_app_start(bool lru_purge_enable){

	esp_err_t err;

	if(httpd_handle == NULL){

		httpd_config_t config = HTTPD_DEFAULT_CONFIG();

		/* this is an important option that isn't set up by default.
		 * We could register all URLs one by one, but this would not work while the fake DNS is active */
		config.uri_match_fn = httpd_uri_match_wildcard;
		config.lru_purge_enable = lru_purge_enable;

		/* generate the URLs */
		if(http_root_url == NULL){
			int root_len = strlen(WEBAPP_LOCATION);

			/* all the pages */
			const char page_js[] = "code.js";
			const char page_css[] = "style.css";
			const char page_connect[] = "connect.json";
			const char page_ap[] = "ap.json";
			const char page_status[] = "status.json";

			/* root url, eg "/"   */
			const size_t http_root_url_sz = sizeof(char) * (root_len+1);
			http_root_url = malloc(http_root_url_sz);
			memset(http_root_url, 0x00, http_root_url_sz);
			strcpy(http_root_url, WEBAPP_LOCATION);

			/* redirect url */
			size_t redirect_sz = 22 + root_len + 1; /* strlen(http://255.255.255.255) + strlen("/") + 1 for \0 */
			http_redirect_url = malloc(sizeof(char) * redirect_sz);
			*http_redirect_url = '\0';

			if(root_len == 1){
				snprintf(http_redirect_url, redirect_sz, "http://%s", DEFAULT_AP_IP);
			}
			else{
				snprintf(http_redirect_url, redirect_sz, "http://%s%s", DEFAULT_AP_IP, WEBAPP_LOCATION);
			}

			/* generate the other pages URLs*/
			http_js_url = http_app_generate_url(page_js);
			http_css_url = http_app_generate_url(page_css);
			http_connect_url = http_app_generate_url(page_connect);
			http_ap_url = http_app_generate_url(page_ap);
			http_status_url = http_app_generate_url(page_status);

		}

		err = httpd_start(&httpd_handle, &config);

	    if (err == ESP_OK) {
	        ESP_LOGI(TAG, "Registering URI handlers");
	        httpd_register_uri_handler(httpd_handle, &http_server_get_request);
	        httpd_register_uri_handler(httpd_handle, &http_server_post_request);
	        httpd_register_uri_handler(httpd_handle, &http_server_delete_request);
	    }
	}

}
