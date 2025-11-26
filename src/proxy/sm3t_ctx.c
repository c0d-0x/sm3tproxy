#include <netinet/in.h>
#include "proxy.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>

bool sm3t__append_vec(sm3t_vec_t *vec, sm3t_context_t const ctx[static 1]) {
    if (vec == NULL) {
        if ((vec = malloc(sizeof(sm3t_vec_t) + sizeof(sm3t_context_t) * SM3T_VEC_MAX)) == NULL) SM3T__OUT_OF_MEMORY();
        vec->capacity = SM3T_VEC_MAX;
        vec->size = 0;
    } else if (vec->size == vec->capacity) {
        size_t capacity = vec->capacity * 2;
        sm3t_vec_t *new_vec = realloc(vec, sizeof(sm3t_vec_t) + sizeof(sm3t_context_t) * capacity);
        if (new_vec == NULL) SM3T__OUT_OF_MEMORY();

        vec = new_vec;
        vec->capacity = capacity;
    }

    vec->data[vec->size++] = (sm3t_context_t *) ctx;

    return true;
}

sm3t_context_t *sm3t__pop_vec(sm3t_context_t *ctx);

void cleanup_vec(sm3t_vec_t *vec) {
    for (int i = 0; i < vec->size; i++) {
        sm3r__cleanup_ctx(vec->data[i]);
    }

    vec->size = 0;
}

void destroy_vec(sm3t_vec_t *vec) { free(vec); }

void sm3t__set_peer_info(sm3t_context_t *ctx, struct sockaddr_storage *addr_storage) {
    struct sockaddr_in6 *addr6_nt = NULL;
    struct sockaddr_in *addr_nt = NULL;
    if (addr_storage->ss_family == AF_INET6) {
        addr6_nt = (struct sockaddr_in6 *) addr_storage;
    } else {
        addr_nt = (struct sockaddr_in *) addr_storage;
    }

    void *addr = (addr_storage->ss_family == AF_INET6) ? (void *) &addr6_nt->sin6_addr : (void *) &addr_nt->sin_addr;
    uint32_t port = (addr_storage->ss_family == AF_INET6) ? addr6_nt->sin6_port : addr_nt->sin_port;
    size_t addr_len = (addr_storage->ss_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    ctx->meta.port = ntohs(port);
    inet_ntop(addr_storage->ss_family, addr, ctx->meta.addr, addr_len);

    ctx->meta.port = ntohs(port);
}

int sm3t__connect_to_server(struct sockaddr_storage *server_addr, char *ip, int port) {
    int server_sock = SM3T__ERR;
    if ((server_sock = socket(server_addr->ss_family, SOCK_STREAM, 0)) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to create sever socket: %s\n", strerror(errno));
        return SM3T__ERR;
    }

    if (connect(server_sock, (struct sockaddr *) server_addr, sizeof(*server_addr)) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to connect to sever: %s:%d\n", ip, port);
        fprintf(stderr, "ERROR: %s\n", strerror(errno));
        return SM3T__ERR;
    }

    fprintf(stderr, "INFO: Connection to: %s:%d established\n", ip, port);
    return server_sock;
}
