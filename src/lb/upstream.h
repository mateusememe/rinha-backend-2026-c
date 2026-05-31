#ifndef RINHA_UPSTREAM_H
#define RINHA_UPSTREAM_H
#include <stdint.h>
#include <stdatomic.h>
typedef struct { char host[64]; int port; } upstream_t;
typedef struct { upstream_t hosts[8]; int count; _Atomic uint32_t current; } upstream_pool_t;
void upstream_init(upstream_pool_t *pool);
void upstream_add(upstream_pool_t *pool, const char *host, int port);
upstream_t *upstream_next(upstream_pool_t *pool);
#endif
