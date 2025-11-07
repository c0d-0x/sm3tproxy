#include "proxy.h"

#include <arpa/inet.h>
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

bool set_nonblocking(int fd) {
    int flags = 0;
    if ((flags = fcntl(fd, F_GETFL, 0)) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to get fd flags: %s\n", strerror(errno));
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to set fd to non-blocking: %s\n", strerror(errno));
        return false;
    }

    return true;
}

int init_server(char *port) {
    struct addrinfo *host = nullptr;
    struct addrinfo hint = {};
    int sock_fd;
    int status;

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;

    if ((status = getaddrinfo(nullptr, port, &hint, &host)) != 0) {
        fprintf(stderr, "ERROR: %s", gai_strerror(status));
        return SP_ERR;
    }

    if ((sock_fd = socket(host->ai_family, host->ai_socktype, host->ai_protocol)) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to create a valid socke: %s", strerror(errno));
        freeaddrinfo(host);
        return SP_ERR;
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to set socket options: %s\n", strerror(errno));
        return SP_ERR;
    }

    if (bind(sock_fd, host->ai_addr, host->ai_addrlen) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to bind socket: %s\n", strerror(errno));
        freeaddrinfo(host);
        return SP_ERR;
    }

    if (!set_nonblocking(sock_fd)) return SP_ERR;
    if (listen(sock_fd, BACKLOG) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to listen: %s\n", strerror(errno));
        freeaddrinfo(host);
        return SP_ERR;
    }

    freeaddrinfo(host);
    return sock_fd;
}

static void close_client(Client *client) {
    if (client != nullptr) {
        close(client->fd);
        free(client);
    }
}

static void cleanup_conn(Conn *conn) {
    close_client(conn->client);
    close_client(conn->server);
    free(conn);
    conn = nullptr;
}

static int connect_to_server(struct sockaddr_in *server_addr, socklen_t addr_len) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, (const void *) &server_addr->sin_addr, ip, sizeof(ip));

    int server_sock = SP_ERR;
    if (connect(server_sock, server_addr, addr_len) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to connect to sever: %s:%d\n", ip, ntohs(server_addr->sin_port));
        fprintf(stderr, "ERROR: %s\n", strerror(errno));
        return SP_ERR;
    }

    fprintf(stderr, "INFO: connection to sever: %s:%d established\n", ip, ntohs(server_addr->sin_port));
    return server_sock;
}

bool handle_context(Context *ctx) {
    int n = SP_ERR;
    Conn *conn = ctx->conn;
    if (ctx->active == SERVER) {
        if ((n = recv(conn->server->fd, conn->server->buffer, BUFFER_SIZE, 0)) == SP_ERR) {
            fprintf(stderr, "ERROR: Failed to recv from server: %s\n", strerror(errno));
            return false;
        }

        if (n == 0) {
            fprintf(stderr, "INFO: Connection closed\n");
            cleanup_conn(conn);
            return false;
        }

        conn->server->n = n;
        if ((send(conn->client->fd, conn->client->buffer, BUFFER_SIZE, 0) == SP_ERR)) {
            fprintf(stderr, "ERROR: Failed to send to client: %s\n", strerror(errno));
            return false;
        }

        if (n == 0) {
            fprintf(stderr, "INFO: Connection closed\n");
            cleanup_conn(conn);
            return false;
        }

        if (n != conn->server->n) {
            fprintf(stderr, "INFO: Under send to client\n");
            return false;
        }

    } else {
        if ((n = recv(conn->client->fd, conn->client->buffer, BUFFER_SIZE, 0)) == SP_ERR) {
            fprintf(stderr, "ERROR: Failed to recv from client: %s\n", strerror(errno));
            return false;
        }

        if (n == 0) {
            fprintf(stderr, "INFO: Connection closed\n");
            cleanup_conn(conn);
            return false;
        }
        conn->server->n = n;
        if ((n = send(conn->server->fd, conn->server->buffer, BUFFER_SIZE, 0) < 0)) {
            fprintf(stderr, "ERROR: Failed to send to server: %s\n", strerror(errno));
            return false;
        }

        if (n == 0) {
            fprintf(stderr, "INFO: Connection closed\n");
            cleanup_conn(conn);
            return false;
        }
        if (n != conn->client->n) {
            fprintf(stderr, "INFO: Under send to server\n");
            return false;
        }
    }

    return true;
}
// An echo server for now!
void echo_engine(Conn *conn) {
    assert(conn != nullptr);

    int n = 0;
    if ((conn->client->n = recv(conn->client->fd, conn->client->buffer, BUFFER_SIZE, 0)) == 0) {
        fprintf(stderr, "INFO: connection closed\n");
        cleanup_conn(conn);
        return;
    }

    n = conn->client->n;
    conn->client->buffer[n] = '\0';
    fprintf(stdout, "MSG: %s\n", conn->client->buffer);
    if ((n = send(conn->client->fd, conn->client->buffer, n, 0)) == 0) {
        fprintf(stderr, "INFO: Connection a closed\n");
        cleanup_conn(conn);
        return;
    }
    return;
}

void run_server(char *port) {
    if (port == nullptr) port = DEFAULT_PORT;
    int proxy_server = init_server(port);

    int epoll_fd = 0;
    struct epoll_event ev = {};
    ev.events = EPOLLIN;
    ev.data.fd = proxy_server;
    if ((epoll_fd = epoll_create(BACKLOG)) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to create an epoll instance\n");
        close(proxy_server);
        return;
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, proxy_server, &ev) == SP_ERR) {
        fprintf(stderr, "ERROR: Failed to add fd to epoll list\n");
        close(proxy_server);
        close(epoll_fd);
        return;
    }

    int n = 0;
    struct epoll_event ev_list[MAX_EVENT];
    while (true) {
        if ((n = epoll_wait(epoll_fd, ev_list, MAX_EVENT, -1)) == SP_ERR) {
            fprintf(stderr, "ERROR: Failed to wait for epoll events\n");
            close(proxy_server);
            close(epoll_fd);
            return;
        }

        for (int i = 0; i < n; i++) {
            if (ev_list[i].data.fd == proxy_server) {
                fprintf(stderr, "INFO: Accepting new incoming connection\n");
                Conn *conn = calloc(1, sizeof(Conn));
                if (conn == nullptr) {
                    fprintf(stderr, "ERROR: Failed to allocate memory\n");
                    close(proxy_server);
                    return;
                }

                if ((conn->client = calloc(1, sizeof(Client) + BUFFER_SIZE + 1)) == nullptr) {
                    fprintf(stderr, "ERROR: Failed to allocate memory\n");
                    close(proxy_server);
                    cleanup_conn(conn);
                    return;
                }

                if ((conn->client->fd = accept(proxy_server, nullptr, nullptr)) == SP_ERR) {
                    fprintf(stderr, "ERROR: Failed to accept connection: %s\n", strerror(errno));
                    close(proxy_server);
                    cleanup_conn(conn);
                    continue;
                }

                if (!set_nonblocking(conn->client->fd)) {
                    close(proxy_server);
                    cleanup_conn(conn);
                    return;
                }

                ev.data.ptr = &(Context) {
                    .active = CLIENT,
                    .conn = conn,
                };

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn->client->fd, &ev) == SP_ERR) {
                    fprintf(stderr, "ERROR: Failed to add fd to epoll list\n");
                    close(proxy_server);
                    close(epoll_fd);
                    cleanup_conn(conn);
                    return;
                }

                if ((conn->server = calloc(1, sizeof(Client) + BUFFER_SIZE + 1)) == nullptr) {
                    fprintf(stderr, "ERROR: Failed to allocate memory\n");
                    close(proxy_server);
                    cleanup_conn(conn);
                    return;
                }

                struct sockaddr_in dst_server_addr;
                socklen_t addr_len = sizeof(dst_server_addr);

                if (getsockopt(conn->client->fd, SOL_IP, SO_ORIGINAL_DST, &dst_server_addr, &addr_len) == SP_ERR) {
                    fprintf(stderr, "ERROR: Failed to add fd to epoll list\n");
                    close(proxy_server);
                    close(epoll_fd);
                    cleanup_conn(conn);
                    return;
                }

                conn->server->fd = connect_to_server(&dst_server_addr, addr_len);
                if (conn->server->fd == SP_ERR) {
                    close(proxy_server);
                    close(epoll_fd);
                    cleanup_conn(conn);
                    return;
                }

                if (!set_nonblocking(conn->server->fd)) {
                    close(proxy_server);
                    close(epoll_fd);
                    cleanup_conn(conn);
                    return;
                }

                ev.data.ptr = &(Context) {
                    .active = SERVER,
                    .conn = conn,
                };

                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn->server->fd, &ev) == SP_ERR) {
                    fprintf(stderr, "ERROR: Failed to add fd to epoll list\n");
                    close(proxy_server);
                    close(epoll_fd);
                    cleanup_conn(conn);
                    return;
                }

            } else {
                if (ev_list[0].events & EPOLLIN) {
                    Context *ctx = ev_list[i].data.ptr;
                    // TODO: handle connection here
                    handle_context(ctx);
                }
            }
        }
    }
}
