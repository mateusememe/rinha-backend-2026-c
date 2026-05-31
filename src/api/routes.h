#ifndef RINHA_ROUTES_H
#define RINHA_ROUTES_H

#include "http.h"
#include "common/types.h"

void handle_request(const http_request_t *req, char *resp_buf, size_t *resp_len, ref_store_t *store);

#endif
