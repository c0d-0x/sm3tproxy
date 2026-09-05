#ifndef SM3T_CORE_H
#define SM3T_CORE_H

#include <stdio.h>
#include <sys/epoll.h>

#define SM3T_VERSION "v1.0.0"
#define SM3T__FATAL(...)              \
    do {                              \
        fprintf(stderr, "ERROR: ");   \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
        exit(EXIT_FAILURE);           \
    } while (0)

#define SM3T__OUT_OF_MEMORY() SM3T__FATAL("Out of memory")
#define SM3T__MAYBE_UNUSED [[maybe_unused]]

typedef enum {
    SM3T__OK = 0,
    SM3T__OKK = 1,
    SM3T__ERR = -1,
    SM3T__ERR_TIMEOUT = -2,
    SM3T__ERR_PROTOCOL = -3
} sm3t_status_t;

bool sm3t__remove_poll(int *epoll_fd, int fd);
bool sm3t_modify_poll(int *epoll_fd, int fd, struct epoll_event *ev);
bool sm3t__append_poll(int *epoll_fd, int fd, struct epoll_event *ev);

#endif  // !SM3T_CORE_H
