#include "upstream.h"
#include <string.h>
#include <stdatomic.h>

void upstream_init(upstream_pool_t *pool) {
    pool->count = 0;
}

void upstream_add(upstream_pool_t *pool, const char *host, int port) {
    if (pool->count < 16) {
        strncpy(pool->hosts[pool->count].host, host, 255);
        pool->hosts[pool->count].port = port;
        pool->count++;
    }
}

upstream_t *upstream_next(upstream_pool_t *pool) {
    static _Atomic int next = 0;
    if (!pool || pool->count == 0) return NULL;
    return &pool->hosts[atomic_fetch_add(&next, 1) % pool->count];
}
