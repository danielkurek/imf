#ifndef HTTP_COMMON_H_
#define HTTP_COMMON_H_

#include <esp_http_server.h>

long int http_load_post_req_to_buf(httpd_req_t *req, char *buf, size_t buf_size);

#endif