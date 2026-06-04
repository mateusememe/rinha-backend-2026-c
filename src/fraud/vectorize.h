#ifndef RINHA_VECTORIZE_H
#define RINHA_VECTORIZE_H

#include "common/types.h"

// Returns 1 if borderline, 0 if obvious (fraud or legit)
int is_borderline(const tx_payload_t *payload);

void vectorize_payload(const tx_payload_t *payload, rinha_vec_t out);
int get_bucket_key(const rinha_vec_t vector);

#endif
