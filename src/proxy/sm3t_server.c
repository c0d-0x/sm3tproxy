#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <sched.h>
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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "logger.h"
#include "sm3t_conf.h"
#include "sm3t_core.h"
#include "sm3t_proxy.h"
#include "sm3t_utils.h"

static void sm3t__close_server(sm3t_server_t *server) {
    if (server == NULL) return;
    close(server->sock);
    free(server);
}

static sm3t_server_t *sm3t__init_server(sm3t_conf_t *conf) {
    struct addrinfo *host = NULL;
    sm3t_server_t *server = NULL;
    struct addrinfo hint = {};
    int sock = SM3T__ERR;
    int status = SM3T__ERR;

    char port[8] = {};
    sprintf(port, "%u", conf->tcp.listen.port);

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, port, &hint, &host)) != 0) {
        log_error("%s", gai_strerror(status));
        return NULL;
    }

    bool done = false;
    struct addrinfo *tmp = host;
    while (tmp != NULL && !done) {
        if ((sock = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol)) == SM3T__ERR) {
            log_error("Failed to create a valid socke: %s", strerror(errno));
            goto RETRY;
        }

        if (conf->mode == SM3T__MODE_TRANSPARENT) {
            if (setsockopt(sock, SOL_IP, IP_TRANSPARENT, &(int){1}, sizeof(int)) == SM3T__ERR) {
                log_error("Failed to make socket transparent: %s", strerror(errno));
                close(sock);
                goto RETRY;
            }
        }

        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == SM3T__ERR) {
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

            if ((server = malloc(sizeof(sm3t_server_t))) == NULL) {
                close(sock);
                SM3T__OUT_OF_MEMORY();
            }

            *server = (sm3t_server_t){
                .sock = sock,
                .meta.port = ntohs(port),
            };

            inet_ntop(tmp->ai_family, addr, server->meta.addr, tmp->ai_addrlen);
            done = true;
            break;
        }
    RETRY:
        tmp = host->ai_next;
    }

    freeaddrinfo(host);
    if (!done) {
        sm3t__close_server(server);
        return NULL;
    }

    if (!sm3t__set_nonblocking(server->sock)) {
        sm3t__close_server(server);
        return NULL;
    }

    if (listen(server->sock, SM3T_BACKLOG) == SM3T__ERR) {
        log_error("Failed to listen: %s", strerror(errno));
        sm3t__close_server(server);
        return NULL;
    }

    return server;
}

int sm3t__connect_upstream(struct sockaddr_storage *server_addr, char *ip, int port) {
    int sock = SM3T__ERR;
    if ((sock = socket(server_addr->ss_family, SOCK_STREAM, 0)) == SM3T__ERR) {
        log_error("Failed to create sever socket: %s", strerror(errno));
        return SM3T__ERR;
    }

    if (connect(sock, (struct sockaddr *) server_addr, sizeof(*server_addr)) == SM3T__ERR) {
        log_error("Failed to connect to sever: %s:%04u", ip, port);
        log_error("%s", strerror(errno));
        close(sock);
        return SM3T__ERR;
    }

    log_info("Upstream: %s:%04d established", ip, port);
    return sock;
}

void sm3t__run_server(void *config) {
    sm3t_conf_t *conf = (sm3t_conf_t *) config;
    if (conf == NULL) return;

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
    sm3t_queue_t *forward_domains = NULL;
    struct epoll_event ev_list[SM3T_MAX_EVENT] = {};

    if (conf->mode == SM3T__MODE_FORWARD) {
        int count = conf->tcp.forward.upstreams_count;
        for (int i = 0; i < count; i++) {
            sm3t__enqueue(&forward_domains, conf->tcp.forward.upstreams + i);
        }
    }

    log_info("SM3TPROXY RUNNING ON: %s::%04u", server->meta.addr, server->meta.port);
    while (true) {
        int n_events = epoll_wait(epoll_fd, ev_list, SM3T_MAX_EVENT, -1);
        if (n_events == SM3T__ERR) {
            log_error("Failed to make reseption for events");
            break;
        }

        for (int i = 0; i < n_events; i++) {
            if (ev_list[i].data.fd == server->sock) {
                sm3t_context_t *client_ctx = sm3t__new_ctx();

                client_ctx->fd = accept(server->sock, NULL, NULL);
                if (client_ctx->fd == SM3T__ERR) {
                    log_error("Failed accept connection: %s", strerror(errno));
                    continue;
                }

                struct sockaddr_storage client_addr = {};
                socklen_t client_addr_len = sizeof(client_addr);

                if ((getpeername(client_ctx->fd, (struct sockaddr *) &client_addr, &client_addr_len)) == SM3T__ERR) {
                    log_error("Failed to get client addr");
                    sm3t__cleanup_ctx(client_ctx);
                    continue;
                }

                sm3t__set_peer_meta(client_ctx, &client_addr, true);
                log_info("New client connection: %s:%u", client_ctx->meta.addr, client_ctx->meta.port);

                sm3t_context_t *upstream_ctx = sm3t__new_ctx();
                struct sockaddr_storage upstream_addr = {};
                socklen_t upstream_addr_len = sizeof(upstream_addr);
                switch (conf->mode) {
                    case SM3T__MODE_TRANSPARENT:
                        if (getsockopt(client_ctx->fd, SOL_IP, SO_ORIGINAL_DST, (struct sockaddr *) &upstream_addr,
                                       &upstream_addr_len)
                            == SM3T__ERR) {
                            log_error("Failed to get upstream's original dest");
                            sm3t__cleanup_ctx(client_ctx);
                            sm3t__cleanup_ctx(upstream_ctx);

                            continue;
                        }
                        break;

                    case SM3T__MODE_FORWARD:
                        sm3t_upstreams_t *upstream = sm3t__dequeue(forward_domains);
                        if (upstream == NULL) {
                            log_error("Empty forward domain");
                            sm3t__cleanup_ctx(client_ctx);
                            sm3t__cleanup_ctx(upstream_ctx);
                            continue;
                        }

                        if (upstream->ver == IPV4) {
                            // TODO: fetch all upstreams into a circular qeueu, prio
                            struct sockaddr_in addr = {
                                .sin_family = AF_INET,
                                .sin_port = htons(upstream->port),
                            };

                            if (inet_pton(AF_INET, upstream->address, &addr.sin_addr) != SM3T__OKK) {
                                log_error("Failed to prepare upstream address");
                                sm3t__cleanup_ctx(client_ctx);
                                sm3t__cleanup_ctx(upstream_ctx);
                                continue;
                            }

                            upstream_addr = *(struct sockaddr_storage *) &addr;
                        } else {
                            struct sockaddr_in6 addr = {
                                .sin6_addr = AF_INET6,
                                .sin6_port = htons(upstream->port),
                            };

                            if (inet_pton(AF_INET6, upstream->address, &addr.sin6_addr) != SM3T__OKK) {
                                log_error("Failed to prepare upstream address");
                                sm3t__cleanup_ctx(client_ctx);
                                sm3t__cleanup_ctx(upstream_ctx);
                                continue;
                            }

                            upstream_addr = *(struct sockaddr_storage *) &addr;
                        }
                        break;
                    // case SM3T__MODE_SOCKS5:
                    // TODO: socks5 pre-connection workload
                    // break
                    default:
                        sm3t__cleanup_ctx(client_ctx);
                        sm3t__cleanup_ctx(upstream_ctx);
                        continue;
                }

                sm3t__set_peer_meta(upstream_ctx, &upstream_addr, conf->tcp.telemetry.enable);
                upstream_ctx->fd
                    = sm3t__connect_upstream(&upstream_addr, upstream_ctx->meta.addr, upstream_ctx->meta.port);
                if (upstream_ctx->fd == SM3T__ERR) {
                    sm3t__cleanup_ctx(client_ctx);
                    sm3t__cleanup_ctx(upstream_ctx);
                    continue;
                }

                sm3t__set_nonblocking(upstream_ctx->fd);
                ev.events = EPOLLIN | EPOLLRDHUP;

                client_ctx->peer = upstream_ctx;
                upstream_ctx->peer = client_ctx;

                ev.data.ptr = upstream_ctx;
                sm3t__append_poll(&epoll_fd, upstream_ctx->fd, &ev);

                sm3t__set_nonblocking(client_ctx->fd);
                ev.data.ptr = client_ctx;
                sm3t__append_poll(&epoll_fd, client_ctx->fd, &ev);
            } else {
                sm3t_context_t *ctx = ev_list[i].data.ptr;
                ctx->events = ev_list[i].events;

                if (!conf->ctx_vtable[conf->mode](ctx, conf, epoll_fd)) {
                    sm3t__remove_poll(&epoll_fd, ctx->fd);
                    sm3t__vec_append(&dead_ctxs, ctx);
                }
            }
        }

        sm3t__cleanup_vec(dead_ctxs, sm3t__cleanup_ctx);
    }

    close(epoll_fd);
    sm3t__close_server(server);
    sm3t__free_queue(&forward_domains);
    sm3t__cleanup_vec(dead_ctxs, sm3t__cleanup_ctx);
    sm3t__destroy_vec(dead_ctxs);
}
