#ifndef UDS_H
#define UDS_H

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <stddef.h>

int send_fd(int uds_fd, int fd_to_send);
int recv_fd(int uds_fd);

#endif // UDS_H
