#include "proxy.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>

bool sm3t__set_nonblocking(int fd) {
    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to get fd flags: %s\n", strerror(errno));
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to set fd to non-blocking: %s\n", strerror(errno));
        return false;
    }

    return true;
}

sm3t_context_t *sm3t__new_ctx() {
    sm3t_context_t *ctx = malloc(sizeof(sm3t_context_t) + BUFFER_SIZE);
    if (ctx == NULL) SM3T__OUT_OF_MEMORY();
    return ctx;
}

void sm3r__cleanup_ctx(sm3t_context_t *ctx) { free(ctx); }

static bool append_poll(int *epoll_fd, int fd, struct epoll_event *ev) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, fd, ev) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to add fd to epoll list\n");
        fprintf(stderr, "ERROR: Epoll: %s \n", strerror(errno));
        return false;
    }
    return true;
}

static bool remove_poll(int *epoll_fd, int fd) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_DEL, fd, NULL) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to remove fd to epoll list\n");
        fprintf(stderr, "ERROR: Epoll: %s \n", strerror(errno));
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
        fprintf(stderr, "ERROR: %s", gai_strerror(status));
        return SM3T__ERR;
    }

    if ((sock_fd = socket(host->ai_family, host->ai_socktype, host->ai_protocol)) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to create a valid socke: %s", strerror(errno));
        freeaddrinfo(host);
        return SM3T__ERR;
    }

    if (setsockopt(sock_fd, SOL_IP, IP_TRANSPARENT, &(int) {1}, sizeof(int)) == SM3T__ERR
        || setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to set socket options: %s\n", strerror(errno));
        return SM3T__ERR;
    }

    if (bind(sock_fd, host->ai_addr, host->ai_addrlen) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to bind socket: %s\n", strerror(errno));
        freeaddrinfo(host);
        return SM3T__ERR;
    }

    if (!sm3t__set_nonblocking(sock_fd)) {
        freeaddrinfo(host);
        return SM3T__ERR;
    }

    if (listen(sock_fd, BACKLOG) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to listen: %s\n", strerror(errno));
        freeaddrinfo(host);
        return SM3T__ERR;
    }

    freeaddrinfo(host);
    return sock_fd;
}

bool sm3t__handle_context(sm3t_context_t ctx[static 1]) {
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

    n_recv = recv(fd_in, buff, BUFFER_SIZE - 1, 0);
    if (n_recv == SM3T__ERR) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;

        fprintf(stderr, "ERROR: Failed to recv data from %s:%u: %s\n", ip, port, strerror(errno));
        return true;
    }

    if (n_recv == 0) {
        ctx->read_eof = true;

        if (peer != NULL) {
            shutdown(peer->fd, SHUT_WR);
            peer->write_eof = true;

            peer->peer = NULL;
        }

        ctx->peer = NULL;
        fprintf(stderr, "INFO: Node(closed read): %s:%u\n", ip, port);
        return false;
    }

    n_sent = send(fd_out, buff, n_recv, 0);

    if (n_sent == SM3T__ERR) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            ctx->wpartial = true;
            ctx->wstart = 0;
            ctx->wlen = (int) n_recv;

            fprintf(stderr, "WARN: send would block, queued %zd bytes for %s:%u\n", n_recv, ip, port);
            return true;
        }

        fprintf(stderr, "ERROR: Failed to send data to %s:%u: %s\n", ip, port, strerror(errno));
        if (peer != NULL) peer->peer = NULL;

        ctx->peer = NULL;
        return false;
    }

    if (n_sent == 0) {
        if (peer != NULL) {
            peer->write_eof = true;
            peer->peer = NULL;
        }
        ctx->peer = NULL;
        fprintf(stderr, "INFO: send returned 0 (peer closed?) for %s:%u\n", ip, port);
        return false;
    }

    if (n_sent < n_recv) {
        int remaining = (int) (n_recv - n_sent);
        memmove(buff, buff + n_sent, remaining);

        ctx->wpartial = true;
        ctx->wstart = 0;
        ctx->wlen = remaining;

        return true;
    }

    ctx->wpartial = false;
    ctx->wlen = 0;
    ctx->wstart = 0;

    return true;
}

// bool sm3t__handle_context(sm3t_context_t ctx[static 1]) {
//     if (ctx->peer == NULL) {
//         ctx->write_eof = true;
//         ctx->read_eof = true;
//         shutdown(ctx->fd, SHUT_RDWR);
//         return false;
//     }
//
//     char *ip = ctx->meta.addr;
//     uint32_t port = ctx->meta.port;
//
//     int fd_in = ctx->fd;
//     int fd_out = ctx->peer->fd;
//     uint8_t *buff = ctx->buffer;
//     int n_recv = 0;
//     int n_sent = 0;
//
//     if (ctx->events & EPOLLRDHUP) {
//         ctx->read_eof = true;
//         ctx->peer->peer = NULL;
//         ctx->peer = NULL;
//         fprintf(stderr, "INFO: Node(HUP): %s:%d: %s\n", ip, port, strerror(errno));
//         return false;
//     }
//
//     if ((n_recv = recv(fd_in, buff, BUFFER_SIZE - 1, 0)) == SM3T__ERR) {
//         if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;
//         fprintf(stderr, "ERROR: Failed to recv data: %s:%d: %s\n", ip, port, strerror(errno));
//         return true;
//     }
//
//     if (n_recv == 0) {
//         ctx->read_eof = true;
//         ctx->peer = NULL;
//         ctx->peer->peer = NULL;
//         fprintf(stderr, "INFO: Node(closed): %s:%d: %s\n", ip, port, strerror(errno));
//         return false;
//     }
//
//     if ((n_sent = send(fd_out, buff, n_recv, 0)) == SM3T__ERR) {
//         if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;
//         ctx->wpartial = true;
//         ctx->wlen = n_recv;
//         ctx->wstart = 0;
//         fprintf(stderr, "ERROR: Failed to send data: %s:%d: %s\n", ip, port, strerror(errno));
//         return true;
//     }
//
//     if (n_sent == 0) {
//         ctx->peer->write_eof = true;
//         ctx->peer->peer = NULL;
//         ctx->peer = NULL;
//         fprintf(stderr, "INFO: Node(closed): %s:%d: %s\n", ip, port, strerror(errno));
//         return false;
//     }
//
//     if (n_sent != n_recv) {
//         // WARNING: Partial send
//         ctx->wpartial = true;
//         ctx->wlen = n_recv - n_sent;
//         ctx->wstart = n_sent;
//         return true;
//     }
//
//     ctx->wpartial = false;
//     return true;
// }

void sm3t__run_server(char *port) {
    int proxy_server = sm3t__init_server(port);
    if (proxy_server == SM3T__ERR) return;

    int epoll_fd = epoll_create(BACKLOG);
    if (epoll_fd == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to create epoll instance\n");
        close(proxy_server);
        return;
    }

    struct epoll_event ev = {.events = EPOLLIN | EPOLLRDHUP, .data.fd = proxy_server};
    if (!append_poll(&epoll_fd, proxy_server, &ev)) {
        close(proxy_server);
        close(epoll_fd);
        return;
    }

    sm3t_vec_t *dead_ctxs = NULL;
    struct epoll_event ev_list[MAX_EVENT] = {};

    while (1) {
        int n_events = epoll_wait(epoll_fd, ev_list, MAX_EVENT, -1);
        if (n_events == SM3T__ERR) {
            fprintf(stderr, "ERROR: epoll_wait failed\n");
            break;
        }

        for (int i = 0; i < n_events; i++) {
            if (ev_list[i].events & EPOLLIN) {
                if (ev_list[i].data.fd == proxy_server) {
                    fprintf(stderr, "INFO: Accepting new incoming connection\n");

                    int client_sock = accept(proxy_server, NULL, NULL);
                    if (client_sock == SM3T__ERR) {
                        fprintf(stderr, "ERROR: accept failed: %s\n", strerror(errno));
                        continue;
                    }

                    struct sockaddr_storage client_addr = {};
                    socklen_t client_addr_len = sizeof(client_addr);

                    sm3t_context_t *client_ctx = sm3t__new_ctx();
                    sm3t_context_t *server_ctx = sm3t__new_ctx();

                    if ((getsockname(client_sock, (struct sockaddr *) &client_addr, &client_addr_len)) == SM3T__ERR) {
                        fprintf(stderr, "ERROR: getsockopt SO_ORIGINAL_DST failed\n");
                        continue;
                    }

                    sm3t__set_nonblocking(client_sock);
                    sm3t__set_peer_info(client_ctx, &client_addr);

                    struct sockaddr_storage server_addr = {};
                    socklen_t server_addr_len = sizeof(server_addr);
                    if (getsockopt(client_sock, SOL_IP, SO_ORIGINAL_DST, &server_addr, &server_addr_len) == SM3T__ERR) {
                        fprintf(stderr, "ERROR: getsockopt SO_ORIGINAL_DST failed\n");
                        continue;
                    }

                    sm3t__set_peer_info(server_ctx, &server_addr);
                    int server_sock
                        = sm3t__connect_to_server(&server_addr, server_ctx->meta.addr, server_ctx->meta.port);
                    if (server_sock == SM3T__ERR) {
                        continue;
                    }

                    sm3t__set_nonblocking(server_sock);
                    ev.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP;

                    client_ctx->fd = client_sock;
                    ev.data.ptr = client_ctx;
                    append_poll(&epoll_fd, client_ctx->fd, &ev);

                    server_ctx->fd = server_sock;
                    ev.data.ptr = server_ctx;
                    append_poll(&epoll_fd, server_ctx->fd, &ev);

                } else {
                    sm3t_context_t *ctx = ev_list[i].data.ptr;
                    ctx->events = ev_list[i].events;
                    if (!sm3t__handle_context(ctx)) {
                        remove_poll(&epoll_fd, ctx->fd);
                        sm3t__append_vec(dead_ctxs, ctx);
                    }
                }
            }

            // cleanup_vec(dead_ctxs);
        }
    }

    close(proxy_server);
    close(epoll_fd);
}
