#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

extern void test_shm(void);
extern void test_knn(void);
extern void test_vectorize(void);

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_shm);
    RUN_TEST(test_knn);
    RUN_TEST(test_vectorize);
    return UNITY_END();
}
