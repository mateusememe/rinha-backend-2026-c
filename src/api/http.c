#include "http.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

bool parse_http_request(const char *buf, size_t len, http_request_t *req) {
    if (len < 10) return false;
    memset(req, 0, sizeof(http_request_t));
    
    if (strncmp(buf, "GET ", 4) == 0) req->method = METHOD_GET;
    else if (strncmp(buf, "POST ", 5) == 0) req->method = METHOD_POST;
    else return false;

    const char *path_start = strchr(buf, ' ') + 1;
    const char *path_end = strchr(path_start, ' ');
    if (!path_end) return false;
    size_t path_len = path_end - path_start;
    if (path_len >= sizeof(req->path)) path_len = sizeof(req->path) - 1;
    memcpy(req->path, path_start, path_len);
    req->path[path_len] = '\0';

    const char *body_start = strstr(buf, "\r\n\r\n");
    if (body_start) {
        req->body = body_start + 4;
        req->content_length = len - (req->body - buf);
    }
    return true;
}
