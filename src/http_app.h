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

@file http_app.h
@author Tony Pottier
@brief Defines all functions necessary for the HTTP server to run.

Contains the freeRTOS task for the HTTP listener and all necessary support
function to process requests, decode URLs, serve files, etc. etc.

@note http_server task cannot run without the wifi_manager task!
@see https://idyl.io
@see https://github.com/tonyp7/esp32-wifi-manager
*/

#ifndef HTTP_APP_H_INCLUDED
#define HTTP_APP_H_INCLUDED

#include <stdbool.h>
#include <esp_http_server.h>

#ifdef __cplusplus
extern "C" {
#endif


/** @brief Defines the URL where the wifi manager is located
 *  By default it is at the server root (ie "/"). If you wish to add your own webpages
 *  you may want to relocate the wifi manager to another URL, for instance /wifimanager
 */
#define WEBAPP_LOCATION 					CONFIG_WEBAPP_LOCATION


/** 
 * @brief spawns the http server 
 */
void http_app_start(bool lru_purge_enable);

/**
 * @brief stops the http server 
 */
void http_app_stop();

/** 
 * @brief sets a hook into the wifi manager URI handlers. Setting the handler to NULL disables the hook.
 * @return ESP_OK in case of success, ESP_ERR_INVALID_ARG if the method is unsupported.
 */
esp_err_t http_app_set_handler_hook( httpd_method_t method,  esp_err_t (*handler)(httpd_req_t *r)  );


#ifdef __cplusplus
}
#endif

#endif
