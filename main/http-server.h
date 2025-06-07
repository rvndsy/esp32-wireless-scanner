#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include "esp_http_server.h"
httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

#endif //HTTP_SERVER_H_
