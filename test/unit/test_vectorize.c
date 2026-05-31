#include "unity.h"
#include "fraud/vectorize.h"
#include <string.h>

void test_vectorize(void) {
    tx_payload_t payload = {0};
    payload.amount_m = 100000; // 100.0
    payload.customer_avg_amount_m = 100000; // 100.0
    payload.installments = 1;
    payload.terminal_is_online = 1;
    
    rinha_vec_t out;
    vectorize_transaction(&payload, out);
    
    TEST_ASSERT_EQUAL_INT16(100, out[0]); // 100/10000 * 10000 = 100
    TEST_ASSERT_EQUAL_INT16(833, out[1]); // 1/12 * 10000 = 833
    TEST_ASSERT_EQUAL_INT16(1000, out[2]); // ratio 1.0 * 1000 = 1000
    TEST_ASSERT_EQUAL_INT16(SCALE_FACTOR, out[9]); // is_online
}
