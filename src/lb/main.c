#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include "common/uds.h"

#define MAX_EVENTS 1024
#define BACKLOG 8192

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int connect_uds(const char *path) {
    int s = socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) return -1;
    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(s);
        return -1;
    }
    return s;
}

int main(int argc, char **argv) {
    int port = 9999;
    printf("Starting UDS Load Balancer on port %d...\n", port);

    // Get upstream UDS paths from env
    const char *api1_path = getenv("API1_SOCKET");
    if (!api1_path) api1_path = "/sockets/api1.sock";
    const char *api2_path = getenv("API2_SOCKET");
    if (!api2_path) api2_path = "/sockets/api2.sock";

    // Wait and connect to upstreams
    int u1 = -1, u2 = -1;
    while (u1 < 0) {
        u1 = connect_uds(api1_path);
        if (u1 < 0) { printf("Waiting for %s...\n", api1_path); usleep(100000); }
    }
    while (u2 < 0) {
        u2 = connect_uds(api2_path);
        if (u2 < 0) { printf("Waiting for %s...\n", api2_path); usleep(100000); }
    }
    printf("Connected to upstreams!\n");

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1; setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_addr.s_addr = INADDR_ANY, .sin_port = htons(port) };
    if (bind(sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }
    if (listen(sfd, BACKLOG) < 0) {
        perror("listen");
        return 1;
    }
    set_nonblocking(sfd);

    int epfd = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = sfd };
    epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);

    struct epoll_event events[MAX_EVENTS];
    int next_api = 0;

    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if (events[i].data.fd == sfd) {
                while (1) {
                    int cfd = accept(sfd, NULL, NULL);
                    if (cfd < 0) break;
                    
                    int flag = 1;
                    setsockopt(cfd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
                    setsockopt(cfd, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(int));

                    int target = (next_api++ & 1) ? u2 : u1;
                    send_fd(target, cfd);
                    close(cfd); // Close our copy, it lives in the API container now
                }
            }
        }
    }
    return 0;
}
