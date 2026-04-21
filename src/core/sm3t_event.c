#include <errno.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>

#include "core.h"
#include "logger.h"

bool sm3t__set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == SM3T__ERR) {
        log_error("Failed to get fd flags: %s", strerror(errno));
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == SM3T__ERR) {
        log_error("Failed to set fd to non-blocking: %s", strerror(errno));
        return false;
    }
    return true;
}

bool sm3t__append_poll(int *epoll_fd, int fd, struct epoll_event *ev) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, fd, ev) == SM3T__ERR) {
        log_error("Failed to add fd to epoll list: %s", strerror(errno));
        return false;
    }
    return true;
}

bool sm3t__remove_poll(int *epoll_fd, int fd) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_DEL, fd, NULL) == SM3T__ERR) {
        log_error("Failed to remove fd to epoll list: %s", strerror(errno));
        return false;
    }
    return true;
}

bool sm3t_modify_poll(int *epoll_fd, int fd, struct epoll_event *ev) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_MOD, fd, ev) == SM3T__ERR) {
        log_error("Failed to modify fd in epoll list: %s", strerror(errno));
        return false;
    }
    return true;
}
