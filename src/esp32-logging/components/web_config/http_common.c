#include "http_common.h"

long int http_load_post_req_to_buf(httpd_req_t *req, char *buf, size_t buf_size){
    size_t total_len = req->content_len;
    size_t current_len = 0;
    size_t received = 0;
    if (total_len >= buf_size) {
        /* Respond with 500 Internal Server Error */
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "content too long");
        return -1;
    }
    while (current_len < total_len) {
        received = httpd_req_recv(req, buf + current_len, total_len);
        if (received <= 0) {
            /* Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post value");
            return -2;
        }
        current_len += received;
    }
    buf[total_len] = '\0';
    return current_len;
}