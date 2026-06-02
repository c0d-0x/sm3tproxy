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
#include "sm3t_core.h"
#include "sm3t_proxy.h"

// TODO: Config integration

sm3t_context_t *sm3t__new_ctx() {
    sm3t_context_t *ctx = malloc(sizeof(sm3t_context_t) + SM3T_BUFFER_SIZE);
    if (ctx == NULL) SM3T__OUT_OF_MEMORY();
    return ctx;
}

void sm3t__cleanup_ctx(void *context) {
    sm3t_context_t *ctx = (sm3t_context_t *) context;
    log_info("Connection dropped: %s:%04u", ctx->meta.addr, ctx->meta.port);
    if (ctx->fd > 0) close(ctx->fd);
    free(ctx);
}

void sm3t__set_peer_meta(sm3t_context_t *ctx, struct sockaddr_storage *addr_storage, bool debug) {
    struct sockaddr_in6 *addr6_nt = NULL;
    struct sockaddr_in *addr_nt = NULL;

    if (addr_storage->ss_family == AF_INET6) addr6_nt = (struct sockaddr_in6 *) addr_storage;
    else addr_nt = (struct sockaddr_in *) addr_storage;

    // TODO: Move do display logic in loger instead
    if (!debug) {
        strcpy(ctx->meta.addr, "<<IP>> ");
        ctx->meta.port = 0x00;
        return;
    }

    void *addr = (addr_storage->ss_family == AF_INET6) ? (void *) &addr6_nt->sin6_addr : (void *) &addr_nt->sin_addr;
    uint16_t port = (addr_storage->ss_family == AF_INET6) ? addr6_nt->sin6_port : addr_nt->sin_port;
    size_t addr_len = (addr_storage->ss_family == AF_INET6) ? sizeof(struct sockaddr_in6) : sizeof(struct sockaddr_in);

    ctx->meta.port = ntohs(port);
    inet_ntop(addr_storage->ss_family, addr, ctx->meta.addr, addr_len);
}

// TCP context handler
bool sm3t__tcp_ctx_handler(void *context, SM3T__MAYBE_UNUSED void *config, int epoll_fd) {
    // TODO: Needs refactoring or a rewrite. Reallllly bulky :/
    // NOTE: works for now, though :)

    if (context == NULL) return true;
    sm3t_context_t *ctx = (sm3t_context_t *) context;
    ssize_t n_recv = 0;
    ssize_t n_sent = 0;

    if (ctx->peer == NULL) {
        if (!ctx->wpartial) {
            ctx->write_eof = true;
            ctx->read_eof = true;
            return false;
        }
    }

    int fd_in = ctx->fd;
    int fd_out = ctx->peer->fd;
    uint8_t *buff = ctx->buffer;
    char *address = ctx->meta.addr;
    uint32_t port = ctx->meta.port;
    sm3t_context_t *peer = ctx->peer;

    if ((ctx->events & EPOLLOUT) && ctx->wpartial) {
        if ((n_sent = send(ctx->fd, ctx->buffer + ctx->wstart, ctx->wlen, 0)) == SM3T__ERR) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;

            log_error("Queued data not sent %s:%04u: %s", address, port, strerror(errno));
            if (peer != NULL) peer->peer = NULL;
            ctx->peer = NULL;
            return false;
        }

        if (n_sent == 0) {
            ctx->write_eof = true;
            if (peer != NULL) peer->peer = NULL;
            ctx->peer = NULL;
            log_warn("Zero-byte send on queued flush %s:%04u", address, port);
            return false;
        }

        ctx->wstart += n_sent;
        ctx->wlen -= n_sent;

        if (ctx->wlen == 0) {
            ctx->wstart = 0;
            ctx->wpartial = false;

            struct epoll_event ev_self = {.events = EPOLLIN | EPOLLRDHUP, .data.ptr = ctx};
            sm3t_modify_poll(&epoll_fd, ctx->fd, &ev_self);

            if (peer != NULL) {
                struct epoll_event ev_peer = {.events = EPOLLIN | EPOLLRDHUP, .data.ptr = peer};
                sm3t_modify_poll(&epoll_fd, peer->fd, &ev_peer);
            } else {
                return false;
            }

            log_info("Queued data sent %s:%04u", address, port);
        }

        return true;
    }

    if (peer == NULL) return false;

    if ((ctx->events & EPOLLIN) && !peer->wpartial) {
        if ((n_recv = recv(fd_in, buff, SM3T_BUFFER_SIZE - 1, 0)) == SM3T__ERR) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) goto CHECK_HUP;

            log_error("No data recieved %s:%04u: %s", address, port, strerror(errno));
            if (peer != NULL) peer->peer = NULL;
            ctx->peer = NULL;
            return false;
        }

        if (n_recv == 0) {
            ctx->read_eof = true;

            if (peer != NULL) {
                peer->write_eof = true;
                if (!peer->wpartial) shutdown(peer->fd, SHUT_WR);
                peer->peer = NULL;
            }

            ctx->peer = NULL;
            log_warn("Zero-bytes read: %s:%04u Peer closed", address, port);
            return false;
        }

        if ((n_sent = send(fd_out, buff, n_recv, 0)) == SM3T__ERR) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                memcpy(peer->buffer, buff, n_recv);
                peer->wpartial = true;
                peer->wstart = 0;
                peer->wlen = (int) n_recv;

                struct epoll_event ev_self = {.events = EPOLLRDHUP, .data.ptr = ctx};
                sm3t_modify_poll(&epoll_fd, ctx->fd, &ev_self);

                struct epoll_event ev_peer = {.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP, .data.ptr = peer};
                sm3t_modify_poll(&epoll_fd, peer->fd, &ev_peer);

                log_warn("Would block, queued %zd bytes for %s:%04u", n_recv, peer->meta.addr, peer->meta.port);
                return true;
            }

            log_error("No Data sent %s:%04u: %s", address, port, strerror(errno));
            if (peer != NULL) peer->peer = NULL;
            ctx->peer = NULL;
            return false;
        }

        if (n_sent == 0) {
            if (peer != NULL) {
                peer->write_eof = true;
                shutdown(peer->fd, SHUT_WR);
                peer->peer = NULL;
            }

            ctx->peer = NULL;
            log_warn("Zero-byte sent: %s:%04u Peer closed", address, port);
            return false;
        }

        if (n_sent < n_recv) {
            int remaining = (int) (n_recv - n_sent);
            memcpy(peer->buffer, buff + n_sent, remaining);  // TODO: Non-copy alternatives

            peer->wpartial = true;
            peer->wstart = 0;
            peer->wlen = remaining;

            struct epoll_event ev_self = {.events = EPOLLRDHUP, .data.ptr = ctx};
            sm3t_modify_poll(&epoll_fd, ctx->fd, &ev_self);

            struct epoll_event ev_peer = {.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP, .data.ptr = peer};
            sm3t_modify_poll(&epoll_fd, peer->fd, &ev_peer);

            log_warn("Partial send, queued %d bytes for %s:%04u", remaining, peer->meta.addr, peer->meta.port);
            return true;
        }

        return true;
    }

CHECK_HUP:
    if (ctx->events & EPOLLRDHUP) {
        ctx->read_eof = true;

        if (peer != NULL) {
            peer->write_eof = true;
            if (!peer->wpartial) shutdown(peer->fd, SHUT_WR);
            peer->peer = NULL;
        }

        ctx->peer = NULL;
        log_info("Ctx hupped: %s:%04u", address, port);
        return false;
    }

    return true;
}

bool sm3t__tcp_echo(void *context, SM3T__MAYBE_UNUSED void *config, SM3T__MAYBE_UNUSED int epoll_fd) {
    sm3t_context_t *ctx = (sm3t_context_t *) context;
    uint8_t *buf = ctx->buffer;
    if (ctx->events & EPOLLRDHUP) {
        log_info("Ctx hupped: %s:%u", ctx->meta.addr, ctx->meta.port);
        return false;
    }

    if (ctx->events && EPOLLIN) {
        int n_rcv = recv(ctx->fd, buf, SM3T_BUFFER_SIZE - 1, 0);
        if (n_rcv > 0) {
            int n_sent = 0;
            send(ctx->fd, "ECHO: ", 7, 0);
            do {
                n_sent = send(ctx->fd, ctx->buffer, n_rcv, 0);
            } while (n_sent != n_rcv);
        } else {
            log_error("Zero-bytes recieved from: %s:%u", ctx->meta.addr, ctx->meta.port);
            return false;
        }
    }

    return true;
}

bool sm3t__socks5__ctx_handler(SM3T__MAYBE_UNUSED void *context, SM3T__MAYBE_UNUSED void *config,
                               SM3T__MAYBE_UNUSED int epoll_fd) {
    log_warn("SOCKS5 HANDLER NOT IMPLEMENTED");
    return false;
}
