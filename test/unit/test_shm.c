#include "unity.h"
#include "common/shm.h"
#include <string.h>
#include <sys/mman.h>

void test_shm(void) {
    const char *test_name = "test_shm_speckit";
    ref_store_t *store = shm_create(test_name);
    TEST_ASSERT_NOT_NULL(store);
    
    store->magic = SHM_MAGIC;
    store->count = 42;
    
    ref_store_t *attached = shm_attach(test_name);
    TEST_ASSERT_NOT_NULL(attached);
    TEST_ASSERT_EQUAL_UINT64(SHM_MAGIC, attached->magic);
    TEST_ASSERT_EQUAL_UINT32(42, attached->count);
    
    shm_detach(attached);
    shm_detach(store);
    shm_unlink(test_name);
}
