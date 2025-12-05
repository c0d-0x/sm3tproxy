
#include "proxy.h"

#ifndef LOGGER_IMPL
#define LOGGER_IMPL
#endif

#ifndef LOG_USE_COLOR
#define LOG_USE_COLOR
#endif

#include "logger.h"

#include <errno.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/socket.h>

bool sm3t__set_nonblocking(int fd) {
    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) == SM3T__ERR) {
        log_error("Failed to get fd flags: %s", strerror(errno));
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == SM3T__ERR) {
        log_error("Failed to set fd to non-blocking: %s", strerror(errno));
        return false;
    }

    return true;
}

bool sm3t__append_poll(int *epoll_fd, int fd, struct epoll_event *ev) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, fd, ev) == SM3T__ERR) {
        log_error("Failed to add fd to epoll list: %s", strerror(errno));
        return false;
    }
    return true;
}

bool sm3t__remove_poll(int *epoll_fd, int fd) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_DEL, fd, NULL) == SM3T__ERR) {
        log_error("Failed to remove fd to epoll list");
        log_error("Epoll: %s ", strerror(errno));
        return false;
    }
    return true;
}

bool sm3t_modify_poll(int *epoll_fd, int fd, struct epoll_event *ev) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_MOD, fd, ev) == SM3T__ERR) {
        log_error("Failed to modify fd in epoll list: %s", strerror(errno));
        return false;
    }
    return true;
}

int sm3t__init_server(char *port) {
    struct addrinfo *host = NULL;
    struct addrinfo hint = {};
    int sock_fd;
    int status;

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(NULL, port, &hint, &host)) != 0) {
        log_error("%s", gai_strerror(status));
        return SM3T__ERR;
    }

    if ((sock_fd = socket(host->ai_family, host->ai_socktype, host->ai_protocol)) == SM3T__ERR) {
        log_error("Failed to create a valid socke: %s", strerror(errno));
        freeaddrinfo(host);
        return SM3T__ERR;
    }

    if (setsockopt(sock_fd, SOL_IP, IP_TRANSPARENT, &(int) {1}, sizeof(int)) == SM3T__ERR
        || setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) == SM3T__ERR) {
        log_error("Failed to set socket options: %s", strerror(errno));
        return SM3T__ERR;
    }

    if (bind(sock_fd, host->ai_addr, host->ai_addrlen) == SM3T__ERR) {
        log_error("Failed to bind socket: %s", strerror(errno));
        freeaddrinfo(host);
        return SM3T__ERR;
    }

    if (!sm3t__set_nonblocking(sock_fd)) {
        freeaddrinfo(host);
        return SM3T__ERR;
    }

    if (listen(sock_fd, BACKLOG) == SM3T__ERR) {
        log_error("Failed to listen: %s", strerror(errno));
        freeaddrinfo(host);
        return SM3T__ERR;
    }

    freeaddrinfo(host);
    return sock_fd;
}

void sm3t__run_server(char *port) {
    int proxy_server = sm3t__init_server(port);
    if (proxy_server == SM3T__ERR) return;

    int epoll_fd = epoll_create(BACKLOG);
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
    struct epoll_event ev_list[MAX_EVENT] = {};

    log_info("SM3TPROXY RUNNING ON PORT: %s", port);
    while (true) {
        int n_events = epoll_wait(epoll_fd, ev_list, MAX_EVENT, -1);
        if (n_events == SM3T__ERR) {
            log_error("epoll_wait failed");
            break;
        }

        for (int i = 0; i < n_events; i++) {
            if (ev_list[i].data.fd == proxy_server) {
                log_info("New incoming connection");

                int client_sock = accept(proxy_server, NULL, NULL);
                if (client_sock == SM3T__ERR) {
                    log_error("accept failed: %s", strerror(errno));
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
                sm3t__set_peer_meta(client_ctx, &client_addr);

                struct sockaddr_storage server_addr = {};
                socklen_t server_addr_len = sizeof(server_addr);
                if (getsockopt(client_sock, SOL_IP, SO_ORIGINAL_DST, (struct sockaddr *) &server_addr, &server_addr_len)
                    == SM3T__ERR) {
                    log_error("getsockopt SO_ORIGINAL_DST failed");
                    continue;
                }

                sm3t__set_peer_meta(server_ctx, &server_addr);
                int server_sock = sm3t__connect_to_server(&server_addr, server_ctx->meta.addr, server_ctx->meta.port);
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
                if (!sm3t__handle_context(ctx, epoll_fd)) {
                    sm3t__remove_poll(&epoll_fd, ctx->fd);
                    sm3t__vec_append(&dead_ctxs, ctx);
                }
            }
        }

        sm3t__cleanup_vec(dead_ctxs);
    }

    close(proxy_server);
    close(epoll_fd);
}
