#include <netinet/in.h>
#include "proxy.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

sm3t_context_t *sm3t__new_ctx() {
    sm3t_context_t *ctx = malloc(sizeof(sm3t_context_t) + BUFFER_SIZE);
    if (ctx == NULL) SM3T__OUT_OF_MEMORY();
    return ctx;
}

void sm3t__cleanup_ctx(sm3t_context_t *ctx) {
    fprintf(stderr, "INFO: cleaning up ctx: %s:%u\n", ctx->meta.addr, ctx->meta.port);
    free(ctx);
}

bool sm3t__vec_append(sm3t_vec_t **vec, sm3t_context_t *ctx) {
    if (ctx == NULL) return false;
    if (*vec == NULL) {
        if ((*vec = malloc(sizeof(sm3t_vec_t) + sizeof(sm3t_context_t *) * SM3T_VEC_MAX)) == NULL)
            SM3T__OUT_OF_MEMORY();
        (*vec)->capacity = SM3T_VEC_MAX;
        (*vec)->size = 0;
    } else if ((*vec)->size == (*vec)->capacity) {
        size_t capacity = (*vec)->capacity * 2;
        sm3t_vec_t *new_vec = realloc(*vec, sizeof(sm3t_vec_t) + sizeof(sm3t_context_t *) * capacity);
        if (new_vec == NULL) SM3T__OUT_OF_MEMORY();

        *vec = new_vec;
        (*vec)->capacity = capacity;
    }

    (*vec)->data[(*vec)->size++] = (sm3t_context_t *) ctx;

    return true;
}

void sm3t__cleanup_vec(sm3t_vec_t *vec) {
    if (vec == NULL) return;
    for (size_t i = 0; i < vec->size; i++) {
        sm3t__cleanup_ctx(vec->data[i]);
    }

    vec->size = 0;
}

void sm3t__destroy_vec(sm3t_vec_t *vec) { free(vec); }

void sm3t__set_peer_meta(sm3t_context_t *ctx, struct sockaddr_storage *addr_storage) {
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

bool sm3t__handle_context(sm3t_context_t ctx[static 1], int epoll_fd) {
    ssize_t n_recv = 0;
    ssize_t n_sent = 0;

    if (ctx->peer == NULL) {
        ctx->write_eof = true;
        ctx->read_eof = true;
        shutdown(ctx->fd, SHUT_RDWR);
        return false;
    }

    char *ip = ctx->meta.addr;
    uint32_t port = ctx->meta.port;
    int fd_in = ctx->fd;
    sm3t_context_t *peer = ctx->peer;
    int fd_out = peer->fd;
    uint8_t *buff = ctx->buffer;

    if (ctx->events & EPOLLRDHUP) {
        ctx->read_eof = true;

        if (peer != NULL) {
            peer->write_eof = true;
            shutdown(peer->fd, SHUT_WR);

            peer->peer = NULL;
        }

        ctx->peer = NULL;
        fprintf(stderr, "INFO: Node(HUP): %s:%u\n", ip, port);
        return false;
    }

    if ((ctx->events & EPOLLOUT) && ctx->wpartial) {
        if ((n_sent = send(ctx->fd, ctx->buffer + ctx->wstart, ctx->wlen, 0)) == SM3T__ERR) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;

            fprintf(stderr, "ERROR: Failed to send queued data to %s:%u: %s\n", ip, port, strerror(errno));
            if (peer != NULL) peer->peer = NULL;

            ctx->peer = NULL;
            return false;
        }

        if (n_sent == 0) {
            shutdown(ctx->fd, SHUT_WR);
            ctx->write_eof = true;

            if (peer != NULL) peer->peer = NULL;

            ctx->peer = NULL;
            fprintf(stderr, "INFO: Peer closed write %s:%u\n", ip, port);
            return false;
        }

        ctx->wstart += n_sent;
        ctx->wlen -= n_sent;

        if (ctx->wlen == 0) {
            ctx->wpartial = false;
            ctx->wstart = 0;

            struct epoll_event ev_self = {.events = EPOLLIN | EPOLLRDHUP, .data.ptr = ctx};
            sm3t_modify_poll(&epoll_fd, ctx->fd, &ev_self);

            struct epoll_event ev_peer = {.events = EPOLLIN | EPOLLRDHUP, .data.ptr = peer};
            sm3t_modify_poll(&epoll_fd, peer->fd, &ev_peer);

            fprintf(stderr, "INFO: Finished sending queued data to %s:%u\n", ip, port);
        }

        return true;
    }

    if (!(ctx->events & EPOLLIN) || peer->wpartial) return true;
    if ((n_recv = recv(fd_in, buff, BUFFER_SIZE - 1, 0)) == SM3T__ERR) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;

        fprintf(stderr, "ERROR: Failed to recv data from %s:%u: %s\n", ip, port, strerror(errno));
        return false;
    }

    if (n_recv == 0) {
        ctx->read_eof = true;

        if (peer != NULL) {
            shutdown(peer->fd, SHUT_WR);
            peer->write_eof = true;

            peer->peer = NULL;
        }

        ctx->peer = NULL;
        fprintf(stderr, "INFO: Peer closed read: %s:%u\n", ip, port);
        return false;
    }

    if ((n_sent = send(fd_out, buff, n_recv, 0)) == SM3T__ERR) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            memcpy(peer->buffer, buff, n_recv);
            peer->wpartial = true;
            peer->wstart = 0;
            peer->wlen = (int) n_recv;

            // NOTE: Stop reading from this socket until peer's buffer is clear (For now)
            struct epoll_event ev_self = {.events = EPOLLRDHUP, .data.ptr = ctx};
            sm3t_modify_poll(&epoll_fd, ctx->fd, &ev_self);

            // NOTE: Enable EPOLLOUT on peer
            struct epoll_event ev_peer = {.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP, .data.ptr = peer};
            sm3t_modify_poll(&epoll_fd, peer->fd, &ev_peer);

            fprintf(stderr, "WARN: Would block, queued %zd bytes for %s:%u\n", n_recv, peer->meta.addr,
                    peer->meta.port);
            return true;
        }

        fprintf(stderr, "ERROR: Failed to send data to %s:%u: %s\n", ip, port, strerror(errno));
        if (peer != NULL) peer->peer = NULL;

        ctx->peer = NULL;
        return true;
    }

    if (n_sent == 0) {
        if (peer != NULL) {
            shutdown(peer->fd, SHUT_WR);
            peer->write_eof = true;
            peer->peer = NULL;
        }

        ctx->peer = NULL;
        fprintf(stderr, "INFO: Peer closed write %s:%u\n", ip, port);
        return false;
    }

    if (n_sent < n_recv) {
        int remaining = (int) (n_recv - n_sent);
        memcpy(peer->buffer, buff + n_sent, remaining);

        peer->wpartial = true;
        peer->wstart = 0;
        peer->wlen = remaining;

        // NOTE: Stop reading from this socket until peer's buffer is clear (For now)
        struct epoll_event ev_self = {.events = EPOLLRDHUP, .data.ptr = ctx};
        sm3t_modify_poll(&epoll_fd, ctx->fd, &ev_self);

        // NOTE: Enable EPOLLOUT on peer
        struct epoll_event ev_peer = {.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP, .data.ptr = peer};
        sm3t_modify_poll(&epoll_fd, peer->fd, &ev_peer);

        fprintf(stderr, "WARN: Partial send, queued %d bytes for %s:%u\n", remaining, peer->meta.addr, peer->meta.port);
        return true;
    }

    ctx->wpartial = false;
    ctx->wlen = 0;
    ctx->wstart = 0;

    return true;
}
