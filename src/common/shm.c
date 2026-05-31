#include "shm.h"
#include "utils.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef __APPLE__
#define DATASET_FILE "/tmp/rinha_refstore_v5.bin"
#else
#define DATASET_FILE "/app/data/rinha_refstore_v5.bin"
#endif

ref_store_t *shm_create(void) {
    size_t size = sizeof(ref_store_t);
    // Ensure directory exists
    mkdir("/app/data", 0777);
    int fd = open(DATASET_FILE, O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (fd == -1) {
        log_error("\"msg\":\"open create failed\",\"path\":\"%s\",\"error\":\"%s\"", DATASET_FILE, strerror(errno));
        return NULL;
    }
    if (ftruncate(fd, size) == -1) {
        log_error("\"msg\":\"ftruncate failed\",\"error\":\"%s\"", strerror(errno));
        close(fd);
        return NULL;
    }
    ref_store_t *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED) {
        log_error("\"msg\":\"mmap failed\",\"error\":\"%s\"", strerror(errno));
        return NULL;
    }
    memset(ptr, 0, size);
    return ptr;
}

ref_store_t *shm_attach(void) {
    size_t size = sizeof(ref_store_t);
    int fd = open(DATASET_FILE, O_RDONLY, 0);
    if (fd == -1) return NULL;
    ref_store_t *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);
    if (ptr == MAP_FAILED) return NULL;
    return ptr;
}

void shm_detach(ref_store_t *store) {
    if (store) munmap(store, sizeof(ref_store_t));
}
