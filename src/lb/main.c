#include "proxy.h"
#include "upstream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <port> <upstream1:port1> [upstream2:port2...]\n", argv[0]);
        return 1;
    }
    int port = atoi(argv[1]);
    upstream_pool_t pool;
    upstream_init(&pool);
    for (int i = 2; i < argc; i++) {
        char *arg = argv[i];
        char *p = strchr(arg, ':');
        if (p) {
            *p = '\0';
            upstream_add(&pool, arg, atoi(p + 1));
        }
    }
    start_lb(port, &pool);
    return 0;
}
