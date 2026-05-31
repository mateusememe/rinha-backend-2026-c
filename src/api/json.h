#ifndef RINHA_JSON_H
#define RINHA_JSON_H
#include "common/types.h"
#include <stddef.h>
bool parse_transaction_json(const char *json, size_t len, tx_payload_t *out);
void serialize_knn_result(const knn_result_t *res, char *out, size_t max_len);
#endif
