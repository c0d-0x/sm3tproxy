#include "core.h"
#include "proxy.h"
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "logger.h"

sm3t_context_t *sm3t__new_ctx() {
    sm3t_context_t *ctx = malloc(sizeof(sm3t_context_t) + BUFFER_SIZE);
    if (ctx == NULL) SM3T__OUT_OF_MEMORY();
    return ctx;
}

void sm3t__cleanup_ctx(void *context) {
    sm3t_context_t *ctx = (sm3t_context_t *) context;
    log_info("Cleaning up ctx: %s:%04u", ctx->meta.addr, ctx->meta.port);
    free(ctx);
}

void sm3t__set_peer_meta(sm3t_context_t *ctx, struct sockaddr_storage *addr_storage, bool debug) {
    struct sockaddr_in6 *addr6_nt = NULL;
    struct sockaddr_in *addr_nt = NULL;

    if (addr_storage->ss_family == AF_INET6) addr6_nt = (struct sockaddr_in6 *) addr_storage;
    else addr_nt = (struct sockaddr_in *) addr_storage;

    if (!debug) {
        strcpy(ctx->meta.addr, "<<IP>> ");
        ctx->meta.port = 0x00;
        return;
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
        log_error("Failed to create sever socket: %s", strerror(errno));
        return SM3T__ERR;
    }

    if (connect(server_sock, (struct sockaddr *) server_addr, sizeof(*server_addr)) == SM3T__ERR) {
        log_error("Failed to connect to sever: %s:%04u", ip, port);
        log_error("%s", strerror(errno));
        return SM3T__ERR;
    }

    log_info("Connection to: %s:%04d established", ip, port);
    return server_sock;
}

// Transparent TCP context handler
bool sm3t__ttcp_ctx_handler(void *context, int epoll_fd) {
    if (context == NULL) return true;
    sm3t_context_t *ctx = (sm3t_context_t *) context;
    ssize_t n_recv = 0;
    ssize_t n_sent = 0;

    if (ctx->peer == NULL) {
        ctx->write_eof = true;
        ctx->read_eof = true;
        shutdown(ctx->fd, SHUT_RDWR);
        return false;
    }

    char *address = ctx->meta.addr;
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
        log_info("Ctx hupped: %s:%04u", address, port);
        return false;
    }

    if ((ctx->events & EPOLLOUT) && ctx->wpartial) {
        if ((n_sent = send(ctx->fd, ctx->buffer + ctx->wstart, ctx->wlen, 0)) == SM3T__ERR) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;

            log_error("Failed to send queued data to %s:%04u: %s", address, port, strerror(errno));
            if (peer != NULL) peer->peer = NULL;
            ctx->peer = NULL;
            return false;
        }

        if (n_sent == 0) {
            shutdown(ctx->fd, SHUT_WR);
            ctx->write_eof = true;

            if (peer != NULL) peer->peer = NULL;
            ctx->peer = NULL;

            log_info("Peer closed write %s:%04u", address, port);
            return false;
        }

        ctx->wstart += n_sent;
        ctx->wlen -= n_sent;

        if (ctx->wlen == 0) {
            ctx->wstart = 0;
            ctx->wpartial = false;

            struct epoll_event ev_self = {.events = EPOLLIN | EPOLLRDHUP, .data.ptr = ctx};
            sm3t_modify_poll(&epoll_fd, ctx->fd, &ev_self);

            struct epoll_event ev_peer = {.events = EPOLLIN | EPOLLRDHUP, .data.ptr = peer};
            sm3t_modify_poll(&epoll_fd, peer->fd, &ev_peer);

            log_info("Finished sending queued data to %s:%04u", address, port);
        }

        return true;
    }

    if (!(ctx->events & EPOLLIN) || peer->wpartial) return true;
    if ((n_recv = recv(fd_in, buff, BUFFER_SIZE - 1, 0)) == SM3T__ERR) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;

        log_error("Failed to recv data from %s:%04u: %s", address, port, strerror(errno));
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
        log_info("Peer closed read: %s:%04u", address, port);
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

            log_warn("Would block, queued %zd bytes for %s:%04u", n_recv, peer->meta.addr, peer->meta.port);
            return true;
        }

        log_error("Failed to send data to %s:%04u: %s", address, port, strerror(errno));
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
        log_info("Peer closed write %s:%04u", address, port);
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

        log_warn("Partial send, queued %d bytes for %s:%04u", remaining, peer->meta.addr, peer->meta.port);
        return true;
    }

    ctx->wpartial = false;
    ctx->wlen = 0;
    ctx->wstart = 0;

    return true;
}
