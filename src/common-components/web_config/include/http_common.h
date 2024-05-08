/**
 * @file http_common.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Common http functions for @ref web_config.h
 * @version 0.1
 * @date 2024-05-08
 * 
 * @copyright Copyright (c) 2024
 * 
 */

#ifndef HTTP_COMMON_H_
#define HTTP_COMMON_H_

#include <esp_http_server.h>

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief Load POST request to buffer
 * 
 * @param[in] req HTTP request
 * @param[out] buf output buffer
 * @param[in] buf_size size of @p buf
 * @return long int end of POST request in @p buf
 */
long int http_load_post_req_to_buf(httpd_req_t *req, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif
