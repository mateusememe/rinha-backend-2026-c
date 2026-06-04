#define _GNU_SOURCE
#include "common/shm.h"
#include "common/utils.h"
#include "common/uds.h"
#include "http.h"
#include "routes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>

#define MAX_EVENTS 1024
#define BUF_SIZE 8192

static ref_store_t *g_store = NULL;

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

typedef struct {
    int fd;
    char buf[BUF_SIZE];
    int pos;
} conn_t;

int main(int argc, char **argv) {
    const char *socket_path = getenv("API_SOCKET");
    if (!socket_path) {
        int port = 9001;
        if (argc > 1) port = atoi(argv[1]);
        printf("Starting API in fallback port mode on port %d...\n", port);
        // Fallback for local testing
    } else {
        printf("Starting API UDS listener on %s...\n", socket_path);
    }

    g_store = shm_attach();
    if (!g_store) {
        fprintf(stderr, "Failed to attach SHM\n");
        return 1;
    }

    int epfd = epoll_create1(0);
    
    // We bind a UDS server socket
    int sfd = -1;
    if (socket_path) {
        unlink(socket_path);
        sfd = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr = { .sun_family = AF_UNIX };
        strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
        bind(sfd, (struct sockaddr *)&addr, sizeof(addr));
        listen(sfd, 1024);
        set_nonblocking(sfd);
        
        struct epoll_event ev = { .events = EPOLLIN, .data.fd = sfd };
        epoll_ctl(epfd, EPOLL_CTL_ADD, sfd, &ev);
    }

    struct epoll_event events[MAX_EVENTS];
    conn_t *conns = calloc(4096, sizeof(conn_t));
    if (!conns) {
        fprintf(stderr, "Failed to allocate connection table\n");
        return 1;
    }
    int uds_client = -1;

    while (1) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;
            
            if (fd == sfd) {
                // Accept connection from LB
                uds_client = accept(sfd, NULL, NULL);
                if (uds_client >= 0) {
                    set_nonblocking(uds_client);
                    struct epoll_event ev = { .events = EPOLLIN, .data.fd = uds_client };
                    epoll_ctl(epfd, EPOLL_CTL_ADD, uds_client, &ev);
                }
            } else if (fd == uds_client) {
                // Read FDs from LB
                while (1) {
                    int cfd = recv_fd(uds_client);
                    if (cfd < 0) break; // EAGAIN or error
                    
                    if (cfd >= 4096) {
                        close(cfd);
                        continue;
                    }

                    set_nonblocking(cfd);
                    
                    conns[cfd].fd = cfd;
                    conns[cfd].pos = 0;
                    
                    struct epoll_event ev = { .events = EPOLLIN | EPOLLET, .data.fd = cfd };
                    epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
                }
            } else {
                // Read from actual HTTP client
                int cfd = fd;
                conn_t *c = &conns[cfd];
                while (1) {
                    int r = read(cfd, c->buf + c->pos, BUF_SIZE - 1 - c->pos);
                    if (r > 0) {
                        c->pos += r;
                        c->buf[c->pos] = '\0';
                        
                        int start = 0;
                        while (start < c->pos) {
                            http_request_t req;
                            if (parse_http_request(c->buf + start, c->pos - start, &req)) {
                                char resp[BUF_SIZE];
                                size_t resp_len;
                                handle_request(&req, resp, &resp_len, g_store);
                                write(cfd, resp, resp_len);
                                
                                int consumed = (req.body - (c->buf + start)) + req.content_length;
                                start += consumed;
                            } else {
                                break;
                            }
                        }
                        
                        if (start > 0) {
                            if (start == c->pos) {
                                c->pos = 0;
                            } else {
                                memmove(c->buf, c->buf + start, c->pos - start);
                                c->pos -= start;
                            }
                        }
                        
                        if (c->pos >= BUF_SIZE - 1) {
                            close(cfd);
                            c->pos = 0;
                            break;
                        }
                    } else if (r == 0) {
                        close(cfd);
                        c->pos = 0;
                        break;
                    } else {
                        if (errno != EAGAIN && errno != EWOULDBLOCK) {
                            close(cfd);
                            c->pos = 0;
                        }
                        break;
                    }
                }
            }
        }
    }
    return 0;
}
