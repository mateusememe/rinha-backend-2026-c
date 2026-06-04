#define _GNU_SOURCE
#include "uds.h"
#include <string.h>

int send_fd(int uds_fd, int fd_to_send) {
    struct msghdr msg = {0};
    char iobuf[1];
    struct iovec io = { .iov_base = iobuf, .iov_len = sizeof(iobuf) };
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd_to_send, sizeof(int));
    
    iobuf[0] = '@';
    return sendmsg(uds_fd, &msg, MSG_NOSIGNAL);
}

int recv_fd(int uds_fd) {
    struct msghdr msg = {0};
    char iobuf[1];
    struct iovec io = { .iov_base = iobuf, .iov_len = sizeof(iobuf) };
    union {
        char buf[CMSG_SPACE(sizeof(int))];
        struct cmsghdr align;
    } u;

    msg.msg_iov = &io;
    msg.msg_iovlen = 1;
    msg.msg_control = u.buf;
    msg.msg_controllen = sizeof(u.buf);

    if (recvmsg(uds_fd, &msg, 0) <= 0) return -1;
    
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_len == CMSG_LEN(sizeof(int)) &&
        cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        int fd;
        memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
        return fd;
    }
    return -1;
}
