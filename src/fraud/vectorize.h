#ifndef RINHA_VECTORIZE_H
#define RINHA_VECTORIZE_H

#include "common/types.h"

void vectorize_payload(const tx_payload_t *payload, rinha_vec_t out);
int get_bucket_key(const rinha_vec_t vector);

#endif
