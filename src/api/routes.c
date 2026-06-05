#include "routes.h"
#include "json.h"
#include "fraud/vectorize.h"
#include "fraud/knn.h"
#include "common/utils.h"
#include <string.h>
#include <stdio.h>

void handle_request(const http_request_t *req, char *resp_buf, size_t *resp_len, ref_store_t *store) {
    if (req->method == METHOD_GET && strcmp(req->path, "/ready") == 0) {
        if (store && store->magic == SHM_MAGIC) {
            *resp_len = snprintf(resp_buf, 4096,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 2\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
                "OK");
        } else {
            *resp_len = snprintf(resp_buf, 4096, "HTTP/1.1 503 Service Unavailable\r\nConnection: keep-alive\r\n\r\n");
        }
    } else if (req->method == METHOD_POST && (strcmp(req->path, "/fraud-score") == 0 || strcmp(req->path, "/eval") == 0)) {
        tx_payload_t payload;
        if (parse_transaction_json(req->body, req->content_length, &payload)) {
            int borderline = is_borderline(&payload);
            int limit = borderline ? 128 : 10;
            char json[1024];
            
            rinha_vec_t qvec;
            vectorize_payload(&payload, qvec);
            knn_result_t res;
            knn_search(store, qvec, limit, &res);
            serialize_knn_result(&res, json, sizeof(json));
            *resp_len = snprintf(resp_buf, 8192,
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: application/json\r\n"
                "Content-Length: %zu\r\n"
                "Connection: keep-alive\r\n"
                "\r\n"
                "%s", strlen(json), json);
        } else {
            *resp_len = snprintf(resp_buf, 4096, "HTTP/1.1 400 Bad Request\r\nConnection: keep-alive\r\n\r\n");
        }
    } else {
        *resp_len = snprintf(resp_buf, 4096, "HTTP/1.1 404 Not Found\r\nConnection: keep-alive\r\n\r\n");
    }
}
