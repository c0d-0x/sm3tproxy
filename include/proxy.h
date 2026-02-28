#ifndef SM3T_PROXY_H
#define SM3T_PROXY_H

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <fcntl.h>
#include <linux/netfilter_ipv4.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "conf.h"

#define SM3T_DEFAULT_PORT 8080
#define SM3T_BACKLOG 10
#define SM3T_BUFFER_SIZE 1024
#define SM3T_MAX_EVENT 5

typedef struct {
    int port;
    char addr[INET6_ADDRSTRLEN];
} sm3t_metadata_t;

typedef struct {
    int sock;
    sm3t_metadata_t meta;
} sm3t_server_t;

typedef struct sm3t_context_t {
    int fd;
    uint32_t events;

    struct sm3t_context_t *peer;
    sm3t_metadata_t meta;

    bool read_eof;
    bool write_eof;

    int wlen;
    int wstart;
    bool wpartial;

    uint8_t buffer[];
} sm3t_context_t;

bool sm3t__set_nonblocking(int fd);
void sm3t__run_server(sm3t_conf_t *conf);
bool sm3t__ttcp_ctx_handler(void *ctx, int epoll_fd);
int sm3t__connect_upstream(struct sockaddr_storage *server_addr, char *ip, int port);
void sm3t__set_peer_meta(sm3t_context_t *ctx, struct sockaddr_storage *addr_storage, bool debug);

sm3t_context_t *sm3t__new_ctx(void);
void sm3t__cleanup_ctx(void *ctx);
#endif  // !SM3T_PROXY_H
