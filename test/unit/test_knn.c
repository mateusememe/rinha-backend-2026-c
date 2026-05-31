#include "unity.h"
#include "fraud/knn.h"
#include <string.h>
#include <stdlib.h>

void test_knn(void) {
    ref_store_t *store = calloc(1, sizeof(ref_store_t));
    store->count = 5;
    
    // Setup bucket 0
    store->buckets[0].start_idx = 0;
    store->buckets[0].count = 5;
    
    for (int i = 0; i < 5; i++) {
        store->records[i].dims[0] = (int16_t)(i * 100);
        store->records[i].is_fraud = (i < 2) ? 1 : 0; // 0, 1 are fraud (2 frauds)
    }
    
    rinha_vec_t query = {0}; // Should match records 0, 1, 2, 3, 4. 2 frauds.
    knn_result_t result;
    
    knn_search(store, query, &result);
    
    TEST_ASSERT_EQUAL_FLOAT(0.4f, result.fraud_score);
    TEST_ASSERT_TRUE(result.approved);
    
    free(store);
}
