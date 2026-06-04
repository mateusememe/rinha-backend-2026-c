#ifndef RINHA_KNN_H
#define RINHA_KNN_H
#include "common/types.h"
void knn_search(const ref_store_t *store, const rinha_vec_t query, int limit, knn_result_t *result);
#endif
