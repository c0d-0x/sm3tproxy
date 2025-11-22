#include "proxy.h"

#include <arpa/inet.h>
#include <asm-generic/errno-base.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/netfilter_ipv4.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

Node *sm3t__new_node(void) {
    Node *node = nullptr;
    if ((node = calloc(1, sizeof(Node) + BUFFER_SIZE)) == nullptr) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        return nullptr;
    }
    return node;
}

void sm3t__close_node(Node *node) {
    close(node->sock);
    free(node);
}

Conn *sm3t__new_conn(void) {
    Conn *conn = nullptr;
    if ((conn = calloc(1, sizeof(Conn))) == nullptr) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        return nullptr;
    }

    conn->refcount = 2;
    return conn;
}

void sm3t__cleanup_conn(Conn *conn) {
    fprintf(stderr, "INFO: Closing connection: %s:%d => %s:%d\n", conn->info.client_ip, conn->info.client_port,
            conn->info.server_ip, conn->info.server_port);
    sm3t__close_node(conn->client);
    sm3t__close_node(conn->server);
    free(conn);
}

void sm3t__set_peer_info(Conn *conn, struct sockaddr_in *server_addr, struct sockaddr_in *client_addr) {
    conn->info.server_port = ntohs(server_addr->sin_port);
    inet_ntop(server_addr->sin_family, (const void *) &server_addr->sin_addr, conn->info.server_ip, INET_ADDRSTRLEN);

    conn->info.client_port = ntohs(server_addr->sin_port);
    inet_ntop(client_addr->sin_family, (const void *) &client_addr->sin_addr, conn->info.client_ip, INET_ADDRSTRLEN);
}

int sm3t__connect_to_server(struct sockaddr_in *server_addr, char *ip, int port) {
    int server_sock = SM3T__ERR;
    if ((server_sock = socket(server_addr->sin_family, SOCK_STREAM, 0)) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to create sever socket: %s\n", strerror(errno));
        return SM3T__ERR;
    }

    if (connect(server_sock, (struct sockaddr *) server_addr, sizeof(struct sockaddr_in)) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to connect to sever: %s:%d\n", ip, port);
        fprintf(stderr, "ERROR: %s\n", strerror(errno));
        return SM3T__ERR;
    }

    fprintf(stderr, "INFO: Connection to: %s:%d established\n", ip, port);
    return server_sock;
}

Context *sm3t__new_context(Conn *conn, Active active) {
    Context *ctx = malloc(sizeof(Context));
    if (ctx == nullptr) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        return nullptr;
    }

    ctx->active = active;
    ctx->conn = conn;
    return ctx;
}

static bool append_poll(int *epoll_fd, int *fd, struct epoll_event *ev) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_ADD, *fd, ev) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to add fd to epoll list\n");
        return false;
    }
    return true;
}

static bool remove_poll(int *epoll_fd, int fd) {
    if (epoll_ctl(*epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to add fd to epoll list\n");
        return false;
    }
    return true;
}

int sm3t__init_server(char *port) {
    struct addrinfo *host = nullptr;
    struct addrinfo hint = {};
    int sock_fd;
    int status;

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(nullptr, port, &hint, &host)) != 0) {
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

bool sm3t__handle_context(Context ctx[static 1]) {
    int n_recv = 0;
    char *ip;
    int port;

    Conn *conn = ctx->conn;
    int sock_in, sock_out;
    char *buff;

    if (ctx->active == SERVER) {
        ip = conn->info.server_ip;
        port = conn->info.server_port;

        sock_in = conn->server->sock;
        sock_out = conn->client->sock;
        buff = conn->server->buffer;
    } else {
        ip = conn->info.client_ip;
        port = conn->info.client_port;

        sock_in = conn->client->sock;
        sock_out = conn->server->sock;
        buff = conn->client->buffer;
    }

    if (ctx->events & EPOLLRDHUP) {
        fprintf(stderr, "INFO: Node(HUP): %s:%d: %s\n", ip, port, strerror(errno));
        return false;
    }

    if ((n_recv = recv(sock_in, buff, BUFFER_SIZE - 1, 0)) == SM3T__ERR) {
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return true;
        fprintf(stderr, "ERROR: Failed to recv data: %s:%d: %s\n", ip, port, strerror(errno));
        return true;
    }

    if (n_recv == 0) {
        fprintf(stderr, "INFO: Node(closed): %s:%d: %s\n", ip, port, strerror(errno));
        return false;
    }

    int n_sent = 0;
    int total_sent = 0;
    int bytes_left = n_recv;
    while (total_sent < n_recv) {
        if ((n_sent = send(sock_out, buff + total_sent, bytes_left, 0)) == SM3T__ERR) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) break;
            fprintf(stderr, "ERROR: Failed to send data: %s:%d: %s\n", ip, port, strerror(errno));
            return true;
        }

        if (n_sent == 0) {
            conn->refcount--;
            fprintf(stderr, "INFO: Node(closed): %s:%d: %s\n", ip, port, strerror(errno));
            return false;
        }

        total_sent += n_sent;
        bytes_left -= n_sent;
    }

    return true;
}

void sm3t__run_server(char *port) {
    if (port == nullptr) port = DEFAULT_PORT;

    int proxy_server = sm3t__init_server(port);
    if (proxy_server == SM3T__ERR) return;

    int epoll_fd = epoll_create(BACKLOG);
    if (epoll_fd == SM3T__ERR) {
        fprintf(stderr, "ERROR: Failed to create epoll instance\n");
        close(proxy_server);
        return;
    }

    struct epoll_event ev = {};
    ev.events = EPOLLIN | EPOLLRDHUP;
    ev.data.fd = proxy_server;
    if (!append_poll(&epoll_fd, &proxy_server, &ev)) {
        close(proxy_server);
        close(epoll_fd);
        return;
    }

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

                    struct sockaddr_in local_adr = {};
                    socklen_t local_addr_len = sizeof(local_adr);

                    if ((getsockname(client_sock, &local_adr, &local_addr_len)) == SM3T__ERR) {
                        fprintf(stderr, "ERROR: getsockopt SO_ORIGINAL_DST failed\n");
                        continue;
                    }

                    sm3t__set_nonblocking(client_sock);
                    Conn *conn = sm3t__new_conn();
                    if (conn == nullptr) {
                        close(client_sock);
                        continue;
                    }

                    conn->client = sm3t__new_node();
                    conn->client->sock = client_sock;
                    struct sockaddr_in dst_addr = {};
                    socklen_t addr_len = sizeof(dst_addr);
                    if (getsockopt(client_sock, SOL_IP, SO_ORIGINAL_DST, &dst_addr, &addr_len) == SM3T__ERR) {
                        fprintf(stderr, "ERROR: getsockopt SO_ORIGINAL_DST failed\n");
                        sm3t__cleanup_conn(conn);
                        continue;
                    }

                    sm3t__set_peer_info(conn, &dst_addr, &local_adr);
                    conn->server = sm3t__new_node();
                    conn->server->sock
                        = sm3t__connect_to_server(&dst_addr, conn->info.server_ip, conn->info.server_port);
                    if (conn->server->sock == SM3T__ERR) {
                        sm3t__cleanup_conn(conn);
                        continue;
                    }

                    sm3t__set_nonblocking(conn->server->sock);
                    Context *client_ctx = sm3t__new_context(conn, CLIENT);
                    Context *server_ctx = sm3t__new_context(conn, SERVER);
                    assert(server_ctx != nullptr);
                    assert(client_ctx != nullptr);

                    client_ctx->peer = server_ctx;
                    server_ctx->peer = client_ctx;
                    ev.events = EPOLLIN | EPOLLRDHUP;
                    ev.data.ptr = client_ctx;
                    append_poll(&epoll_fd, &client_sock, &ev);

                    ev.data.ptr = server_ctx;
                    append_poll(&epoll_fd, &conn->server->sock, &ev);

                } else {
                    Context *ctx = ev_list[i].data.ptr;
                    ctx->events = ev_list[i].events;
                    if (!sm3t__handle_context(ctx)) {
                        Conn *conn = ctx->conn;
                        remove_poll(&epoll_fd, ctx->conn->client->sock);
                        remove_poll(&epoll_fd, ctx->conn->server->sock);
                        sm3t__cleanup_conn(conn);
                        free(ctx->peer);
                        free(ctx);
                    }
                }
            }
        }
    }
    close(proxy_server);
    close(epoll_fd);
}
