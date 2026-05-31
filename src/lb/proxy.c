#define _GNU_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include "proxy.h"

#define NUM_THREADS 32
#define BUF_SIZE 32768

typedef struct {
    int server_fd;
    upstream_pool_t *pool;
} lb_args_t;

static void *lb_worker(void *arg) {
    lb_args_t *args = (lb_args_t *)arg;
    int server_fd = args->server_fd;
    upstream_pool_t *pool = args->pool;
    char buf[BUF_SIZE];
    int flag = 1;
    int next_backend = 0;

    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
        
        int idx = __atomic_fetch_add(&next_backend, 1, __ATOMIC_RELAXED) % pool->count;
        
        int us_fd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(us_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
        
        struct hostent *server = gethostbyname(pool->hosts[idx].host);
        if (server) {
            struct sockaddr_in us_addr;
            memset(&us_addr, 0, sizeof(us_addr));
            us_addr.sin_family = AF_INET;
            memcpy(&us_addr.sin_addr.s_addr, server->h_addr, server->h_length);
            us_addr.sin_port = htons(pool->hosts[idx].port);
            
            if (connect(us_fd, (struct sockaddr *)&us_addr, sizeof(us_addr)) == 0) {
                ssize_t n = read(client_fd, buf, sizeof(buf));
                if (n > 0) {
                    write(us_fd, buf, n);
                    n = read(us_fd, buf, sizeof(buf));
                    if (n > 0) write(client_fd, buf, n);
                }
            }
        }
        close(us_fd);
        close(client_fd);
    }
}

void start_lb(int port, upstream_pool_t *pool) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    
    struct sockaddr_in address = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port) };
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind"); exit(1);
    }
    listen(server_fd, 16384);
    
    printf("LB started on port %d with %d upstreams...\n", port, pool->count);
    
    lb_args_t args = { .server_fd = server_fd, .pool = pool };
    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, lb_worker, &args);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
}
