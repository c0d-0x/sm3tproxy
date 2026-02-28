#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#ifndef LOGGER_IMPL
#define LOGGER_IMPL
#endif

#ifndef LOG_USE_COLOR
#define LOG_USE_COLOR
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "conf.h"
#include "core.h"
#include "logger.h"
#include "proxy.h"

static sm3t_server_t *sm3t__init_server(sm3t_conf_t *conf) {
    struct addrinfo *host = NULL;
    struct addrinfo hint = {};
    sm3t_server_t *server = NULL;

    char port[8] = {};
    sprintf(port, "%u", conf->tcp.listen.port);
    int sock;
    int status;

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, port, &hint, &host)) != 0) {
        log_error("%s", gai_strerror(status));
        return NULL;
    }

    struct addrinfo *tmp = host;
    bool done = false;
    while (tmp != NULL && !done) {
        if ((sock = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol)) == SM3T__ERR) {
            log_error("Failed to create a valid socke: %s", strerror(errno));
            goto RETRY;
        }

        if (conf->mode == SM3T__MODE_TRANSPARENT) {
            if (setsockopt(sock, SOL_IP, IP_TRANSPARENT, &(int) {1}, sizeof(int)) == SM3T__ERR) {
                log_error("Failed to make socket transparent: %s", strerror(errno));
                close(sock);
                goto RETRY;
            }
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) == SM3T__ERR) {
            log_error("Failed server addr reusabl: %s", strerror(errno));
            close(sock);
            goto RETRY;
        }

        if (bind(sock, tmp->ai_addr, tmp->ai_addrlen) == SM3T__ERR) {
            log_error("Failed to bind socket: %s", strerror(errno));
            close(sock);
        } else {
            void *addr = (tmp->ai_family == AF_INET6) ? (void *) &((struct sockaddr_in6 *) (tmp->ai_addr))->sin6_addr
                                                      : (void *) &((struct sockaddr_in *) (tmp->ai_addr))->sin_addr;
            uint16_t port = (tmp->ai_family == AF_INET6) ? ((struct sockaddr_in6 *) (tmp->ai_addr))->sin6_port
                                                         : ((struct sockaddr_in *) (tmp->ai_addr))->sin_port;

            if ((server = malloc(sizeof(sm3t_server_t))) == NULL) SM3T__OUT_OF_MEMORY();
            *server = (sm3t_server_t) {
                .sock = sock,
                .meta.port = ntohs(port),
            };

            inet_ntop(tmp->ai_family, addr, server->meta.addr, tmp->ai_addrlen);
            done = true;
        }
    RETRY:
        tmp = host->ai_next;
    }

    freeaddrinfo(host);
    if (!done) {
        if (server != NULL) free(server);
        return NULL;
    }

    if (!sm3t__set_nonblocking(sock)) {
        if (server != NULL) free(server);
        return NULL;
    }

    if (listen(sock, SM3T_BACKLOG) == SM3T__ERR) {
        log_error("Failed to listen: %s", strerror(errno));
        if (server != NULL) free(server);
        return NULL;
    }

    return server;
}

static void sm3t__close_server(sm3t_server_t *server) {
    if (server == NULL) return;
    close(server->sock);
    free(server);
}

void sm3t__run_server(sm3t_conf_t *conf) {
    sm3t_server_t *server = sm3t__init_server(conf);
    if (server == NULL) return;

    int epoll_fd = epoll_create(SM3T_BACKLOG);
    if (epoll_fd == SM3T__ERR) {
        log_error("Failed to create epoll instance");
        sm3t__close_server(server);
        return;
    }

    struct epoll_event ev = {.events = EPOLLIN, .data.fd = server->sock};
    if (!sm3t__append_poll(&epoll_fd, server->sock, &ev)) {
        sm3t__close_server(server);
        close(epoll_fd);
        return;
    }

    sm3t_vec_t *dead_ctxs = NULL;
    struct epoll_event ev_list[SM3T_MAX_EVENT] = {};

    log_info("SM3TPROXY RUNNING ON PORT: %s::%04u", server->meta.addr, server->meta.port);
    while (true) {
        int n_events = epoll_wait(epoll_fd, ev_list, SM3T_MAX_EVENT, -1);
        if (n_events == SM3T__ERR) {
            sm3t__close_server(server);
            sm3t__cleanup_vec(dead_ctxs, sm3t__cleanup_ctx);
            sm3t__destroy_vec(dead_ctxs);
            SM3T__FATAL("Failed to make reseption for events");
        }

        for (int i = 0; i < n_events; i++) {
            if (ev_list[i].data.fd == server->sock) {
                log_info("New incoming connection");

                int client_sock = accept(server->sock, NULL, NULL);
                if (client_sock == SM3T__ERR) {
                    log_error("Failed accept connection: %s", strerror(errno));
                    continue;
                }

                struct sockaddr_storage client_addr = {};
                socklen_t client_addr_len = sizeof(client_addr);

                sm3t_context_t *client_ctx = sm3t__new_ctx();
                sm3t_context_t *upstream_ctx = sm3t__new_ctx();

                if ((getpeername(client_sock, (struct sockaddr *) &client_addr, &client_addr_len)) == SM3T__ERR) {
                    log_error("Failed to get client addr");
                    continue;
                }

                sm3t__set_nonblocking(client_sock);
                sm3t__set_peer_meta(client_ctx, &client_addr, conf->global.logging.log_address);

                struct sockaddr_storage upstream_addr = {};
                socklen_t upstream_addr_len = sizeof(upstream_addr);
                if (getsockopt(client_sock, SOL_IP, SO_ORIGINAL_DST, (struct sockaddr *) &upstream_addr,
                               &upstream_addr_len)
                    == SM3T__ERR) {
                    log_error("Failed to get upstream's original dest");
                    continue;
                }

                sm3t__set_peer_meta(upstream_ctx, &upstream_addr, conf->global.logging.log_address);
                int upstream_sock
                    = sm3t__connect_upstream(&upstream_addr, upstream_ctx->meta.addr, upstream_ctx->meta.port);
                if (upstream_sock == SM3T__ERR) {
                    continue;
                }

                sm3t__set_nonblocking(upstream_sock);
                ev.events = EPOLLIN | EPOLLRDHUP;

                client_ctx->peer = upstream_ctx;
                upstream_ctx->peer = client_ctx;

                client_ctx->fd = client_sock;
                ev.data.ptr = client_ctx;
                sm3t__append_poll(&epoll_fd, client_ctx->fd, &ev);

                upstream_ctx->fd = upstream_sock;
                ev.data.ptr = upstream_ctx;
                sm3t__append_poll(&epoll_fd, upstream_ctx->fd, &ev);

            } else {
                sm3t_context_t *ctx = ev_list[i].data.ptr;
                ctx->events = ev_list[i].events;

                if (!conf->ctx_vtable[conf->mode](ctx, epoll_fd)) {
                    sm3t__remove_poll(&epoll_fd, ctx->fd);
                    sm3t__vec_append(&dead_ctxs, ctx);
                }
            }
        }

        sm3t__cleanup_vec(dead_ctxs, sm3t__cleanup_ctx);
    }

    close(epoll_fd);
    sm3t__close_server(server);
    sm3t__destroy_vec(dead_ctxs);
}
