#include "upstream.h"
#include <string.h>
#include <stdatomic.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

void upstream_init(upstream_pool_t *pool) {
    pool->count = 0;
}

void upstream_add(upstream_pool_t *pool, const char *host, int port) {
    if (pool->count < 8) {
        strncpy(pool->hosts[pool->count].host, host, 63);
        pool->hosts[pool->count].port = port;
        
        struct hostent *server = gethostbyname(host);
        if (!server) {
            fprintf(stderr, "Failed to resolve %s\n", host);
            exit(1);
        }
        
        memset(&pool->hosts[pool->count].addr, 0, sizeof(struct sockaddr_in));
        pool->hosts[pool->count].addr.sin_family = AF_INET;
        memcpy(&pool->hosts[pool->count].addr.sin_addr.s_addr, server->h_addr, server->h_length);
        pool->hosts[pool->count].addr.sin_port = htons(port);
        
        pool->count++;
    }
}

upstream_t *upstream_next(upstream_pool_t *pool) {
    static _Atomic int next = 0;
    if (!pool || pool->count == 0) return NULL;
    return &pool->hosts[atomic_fetch_add(&next, 1) % pool->count];
}
