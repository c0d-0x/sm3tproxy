#ifndef PROXY_H
#define PROXY_H

#include <stddef.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <fcntl.h>
#include <linux/netfilter_ipv4.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef DEFAULT_PORT
#define DEFAULT_PORT "8080"
#endif

#define BACKLOG 10
#define BUFFER_SIZE 1024

#define MAX_EVENT 5
#define SM3T_VEC_MAX 16

#define SM3T__FATAL(...)              \
    do {                              \
        fprintf(stderr, "ERROR: ");   \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
        exit(EXIT_FAILURE);           \
    } while (0)

#define SM3T__OUT_OF_MEMORY() SM3T__FATAL("Process out of memory")

typedef struct {
    int port;
    char addr[INET6_ADDRSTRLEN];
} sm3t_metadata_t;

// NOTE: This context it mean to solve memory ownership with my recent context <=> conn.
typedef struct sm3t_context_t {
    int fd;
    uint32_t events;

    struct sm3t_context_t *peer;
    sm3t_metadata_t meta;

    bool read_eof;
    bool write_eof;

    int wlen;       // len of buffer left from a partial send
    int wstart;     // start index of buffer to start sending
    bool wpartial;  // set to true on partial send/write

    uint8_t buffer[];
} sm3t_context_t;

typedef enum {
    SM3T__OK = 0,
    SM3T__ERR = -1,
    SM3T__ERR_TIMEOUT = -2,
    SM3T__ERR_PROTOCOL = -3
} sm3t_status_t;

typedef struct sm3t_vec {
    int size;
    int capacity;
    sm3t_context_t *data[];
} sm3t_vec_t;

bool sm3t__append_vec(sm3t_vec_t *vec, sm3t_context_t const ctx[static 1]);
sm3t_context_t *sm3t__pop_vec(sm3t_context_t *ctx);
void cleanup_vec(sm3t_vec_t *vec);
void destroy_vec(sm3t_vec_t *vec);

void sm3t__run_server(char *port);
bool sm3t__set_nonblocking(int fd);
int sm3t__connect_to_server(struct sockaddr_storage *server_addr, char *ip, int port);
void sm3t__set_peer_info(sm3t_context_t *ctx, struct sockaddr_storage *addr_storage);

sm3t_context_t *sm3t__new_ctx(void);
void sm3r__cleanup_ctx(sm3t_context_t *ctx);
#endif  // !PROXY_H
