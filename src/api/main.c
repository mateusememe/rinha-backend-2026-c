#include "common/shm.h"
#include "common/utils.h"
#include "http.h"
#include "routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <pthread.h>

#define BUF_SIZE 16384

static ref_store_t *g_store = NULL;

static void *worker_func(void *arg) {
    int server_fd = (int)(intptr_t)arg;
    char buf[BUF_SIZE];
    char resp[BUF_SIZE];
    int flag = 1;
    while (1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd < 0) continue;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
        ssize_t n = read(client_fd, buf, BUF_SIZE - 1);
        if (n > 0) {
            buf[n] = '\0'; // NULL-TERMINATE FOR ROBUST PARSING
            http_request_t req;
            if (parse_http_request(buf, n, &req)) {
                size_t resp_len;
                handle_request(&req, resp, &resp_len, g_store);
                write(client_fd, resp, resp_len);
            }
        }
        close(client_fd);
    }
}

int main(int argc, char **argv) {
    int port = 9001;
    if (argc > 1) port = atoi(argv[1]);
    printf("Starting API on port %d...\n", port);
    g_store = shm_attach();
    if (!g_store) {
        fprintf(stderr, "Failed to attach SHM\n");
        return 1;
    }
    printf("SHM attached. Magic: %llx, Count: %u\n", (unsigned long long)g_store->magic, g_store->count);
    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(sfd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port) };
    bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(sfd, 8192);
    int num_threads = 100;
    pthread_t threads[100];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 65536); // 64KB stack
    
    for (int i = 0; i < num_threads; i++) pthread_create(&threads[i], &attr, worker_func, (void *)(intptr_t)sfd);
    for (int i = 0; i < num_threads; i++) pthread_join(threads[i], NULL);
    return 0;
}
