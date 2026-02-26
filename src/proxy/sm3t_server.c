#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#include <stdio.h>
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

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "conf.h"
#include "core.h"
#include "logger.h"
#include "proxy.h"

static int sm3t__init_server(sm3t_conf_t *conf) {
    struct addrinfo *host = NULL;
    struct addrinfo hint = {};

    char port[8] = {};
    sprintf(port, "%u", conf->tcp.listen.port);
    int sock_fd;
    int status;

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, port, &hint, &host)) != 0) {
        log_error("%s", gai_strerror(status));
        return SM3T__ERR;
    }

    struct addrinfo *tmp = host;
    bool done = false;
    while (tmp != NULL && !done) {
        if ((sock_fd = socket(tmp->ai_family, tmp->ai_socktype, tmp->ai_protocol)) == SM3T__ERR) {
            log_error("Failed to create a valid socke: %s", strerror(errno));
            goto RETRY;
        }

        if (conf->mode == SM3T__MODE_TRANSPARENT) {
            if (setsockopt(sock_fd, SOL_IP, IP_TRANSPARENT, &(int) {1}, sizeof(int)) == SM3T__ERR) {
                log_error("Failed to make socket transparent: %s", strerror(errno));
                close(sock_fd);
                goto RETRY;
            }
        }

        if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) == SM3T__ERR) {
            log_error("Failed server addr reusabl: %s", strerror(errno));
            close(sock_fd);
            goto RETRY;
        }

        if (bind(sock_fd, tmp->ai_addr, tmp->ai_addrlen) == SM3T__ERR) {
            log_error("Failed to bind socket: %s", strerror(errno));
            close(sock_fd);
        } else {
            done = true;
        }
    RETRY:
        tmp = host->ai_next;
    }

    freeaddrinfo(host);
    if (!done) return SM3T__ERR;
    if (!sm3t__set_nonblocking(sock_fd)) {
        return SM3T__ERR;
    }

    if (listen(sock_fd, SM3T_BACKLOG) == SM3T__ERR) {
        log_error("Failed to listen: %s", strerror(errno));
        return SM3T__ERR;
    }

    return sock_fd;
}

void sm3t__run_server(sm3t_conf_t *conf) {
    int proxy_server = sm3t__init_server(conf);
    if (proxy_server == SM3T__ERR) return;

    int epoll_fd = epoll_create(SM3T_BACKLOG);
    if (epoll_fd == SM3T__ERR) {
        log_error("Failed to create epoll instance");
        close(proxy_server);
        return;
    }

    struct epoll_event ev = {.events = EPOLLIN, .data.fd = proxy_server};
    if (!sm3t__append_poll(&epoll_fd, proxy_server, &ev)) {
        close(proxy_server);
        close(epoll_fd);
        return;
    }

    sm3t_vec_t *dead_ctxs = NULL;
    struct epoll_event ev_list[SM3T_MAX_EVENT] = {};

    log_info("SM3TPROXY RUNNING ON PORT: %04u", conf->tcp.listen.port);
    while (true) {
        int n_events = epoll_wait(epoll_fd, ev_list, SM3T_MAX_EVENT, -1);
        if (n_events == SM3T__ERR) {
            sm3t__cleanup_vec(dead_ctxs, sm3t__cleanup_ctx);
            sm3t__destroy_vec(dead_ctxs);
            SM3T__FATAL("Failed to make reseption for events");
        }

        for (int i = 0; i < n_events; i++) {
            if (ev_list[i].data.fd == proxy_server) {
                log_info("New incoming connection");

                int client_sock = accept(proxy_server, NULL, NULL);
                if (client_sock == SM3T__ERR) {
                    log_error("Failed accept connection: %s", strerror(errno));
                    continue;
                }

                struct sockaddr_storage client_addr = {};
                socklen_t client_addr_len = sizeof(client_addr);

                sm3t_context_t *client_ctx = sm3t__new_ctx();
                sm3t_context_t *server_ctx = sm3t__new_ctx();

                if ((getpeername(client_sock, (struct sockaddr *) &client_addr, &client_addr_len)) == SM3T__ERR) {
                    log_error("Failed to get client addr");
                    continue;
                }

                sm3t__set_nonblocking(client_sock);
                sm3t__set_peer_meta(client_ctx, &client_addr, conf->global.logging.log_address);

                struct sockaddr_storage server_addr = {};
                socklen_t server_addr_len = sizeof(server_addr);
                if (getsockopt(client_sock, SOL_IP, SO_ORIGINAL_DST, (struct sockaddr *) &server_addr, &server_addr_len)
                    == SM3T__ERR) {
                    log_error("Failed to get dst server options");
                    continue;
                }

                sm3t__set_peer_meta(server_ctx, &server_addr, conf->global.logging.log_address);
                int server_sock = sm3t__connect_upstream(&server_addr, server_ctx->meta.addr, server_ctx->meta.port);
                if (server_sock == SM3T__ERR) {
                    continue;
                }

                sm3t__set_nonblocking(server_sock);
                ev.events = EPOLLIN | EPOLLRDHUP;

                client_ctx->peer = server_ctx;
                server_ctx->peer = client_ctx;

                client_ctx->fd = client_sock;
                ev.data.ptr = client_ctx;
                sm3t__append_poll(&epoll_fd, client_ctx->fd, &ev);

                server_ctx->fd = server_sock;
                ev.data.ptr = server_ctx;
                sm3t__append_poll(&epoll_fd, server_ctx->fd, &ev);

            } else {
                sm3t_context_t *ctx = ev_list[i].data.ptr;
                ctx->events = ev_list[i].events;
                // TODO: More generic later on
                if (!conf->ctx_vtable[conf->mode](ctx, epoll_fd)) {
                    sm3t__remove_poll(&epoll_fd, ctx->fd);
                    sm3t__vec_append(&dead_ctxs, ctx);
                }
            }
        }

        sm3t__cleanup_vec(dead_ctxs, sm3t__cleanup_ctx);
    }

    close(epoll_fd);
    close(proxy_server);
    sm3t__destroy_vec(dead_ctxs);
}
