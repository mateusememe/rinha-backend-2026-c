#ifndef RINHA_HTTP_H
#define RINHA_HTTP_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    METHOD_GET,
    METHOD_POST,
    METHOD_UNKNOWN
} http_method_t;

typedef struct {
    http_method_t method;
    char path[256];
    size_t content_length;
    const char *body;
} http_request_t;

bool parse_http_request(const char *buf, size_t len, http_request_t *req);

#endif
