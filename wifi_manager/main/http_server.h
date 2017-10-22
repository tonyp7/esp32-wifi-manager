/**
 * \file http_server.h
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

#ifndef HTTP_SERVER_H_INCLUDED
#define HTTP_SERVER_H_INCLUDED


#define HTTP_SERVER_START_BIT_0	( 1 << 0 )

typedef struct http_parameter http_parameter;
struct http_parameter {
   char *key;
   char *value;
   char *raw;
};

void http_server(void *pvParameters);
void http_server_netconn_serve(struct netconn *conn);
void http_server_url_decode(char *dst, const char *src);
int  http_server_isxdigit(int c);


/*! \brief Decode a HTTP request parameters.
*   \param body raw HTTP request body.
*   \param parameter_count returns the number of parameters found in the request.
*   \return http_parameter object containing all parameters of the request.
*
*  This parses a HTTP request and returns an array of http_parameter.
*  The memory is dynamically allocated and must be freed by a call to http_server_free_parameters.
*/
http_parameter* http_server_decode_parameters(char *body, int *parameter_count);

uint8_t http_server_is_valid_connection_parameter(http_parameter* parameters, int count);


/*! \brief Free the memory dynamically allocated by a http_server_decode_parameters call.
*   \param parameters http_parameter object to be freed.
*/
void http_server_free_parameters(http_parameter* parameters);


void http_server_set_event_start();




#endif
