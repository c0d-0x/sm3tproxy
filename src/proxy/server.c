#include "proxy.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
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
        fprintf(stderr, "Error: Failed to get fd flags: %s\n", strerror(errno));
        return false;
    }

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        fprintf(stderr, "Error: Failed to set fd to non-blocking: %s\n", strerror(errno));
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
        fprintf(stderr, "Error: %s", gai_strerror(status));
        return SP_ERR;
    }

    if ((sock_fd = socket(host->ai_family, host->ai_socktype, host->ai_protocol)) == SP_ERR) {
        fprintf(stderr, "Error: Failed to create a valid socke: %s", strerror(errno));
        freeaddrinfo(host);
        return SP_ERR;
    }

    int yes;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == SP_ERR) {
        fprintf(stderr, "Error: Failed to set socket options: %s\n", strerror(errno));
        return SP_ERR;
    }

    if (bind(sock_fd, host->ai_addr, host->ai_addrlen) == SP_ERR) {
        fprintf(stderr, "Error: Failed to bind socket: %s\n", strerror(errno));
        freeaddrinfo(host);
        return SP_ERR;
    }

    if (!set_nonblocking(sock_fd)) return SP_ERR;
    if (listen(sock_fd, BACKLOG) == SP_ERR) {
        fprintf(stderr, "Error: Failed to listen: %s\n", strerror(errno));
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

// An echo server for now!
void connection_handler(Conn *conn) {
    assert(conn != nullptr);

    int n = 0;
    if ((conn->client->n = recv(conn->client->fd, conn->client->buffer, BUFFER_SIZE, 0)) == 0) {
        fprintf(stderr, "Info: connection closed\n");
        close_client(conn->client);
        free(conn);
        return;
    }

    n = conn->client->n;
    conn->client->buffer[n] = '\0';
    fprintf(stdout, "MSG: %s\n", conn->client->buffer);
    if ((n = send(conn->client->fd, conn->client->buffer, n, 0)) == 0) {
        fprintf(stderr, "Info: Connection a closed\n");
        close_client(conn->client);
        free(conn);
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
        fprintf(stderr, "Error: Failed to create an epoll instance\n");
        close(proxy_server);
        return;
    }

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, proxy_server, &ev) == SP_ERR) {
        fprintf(stderr, "Error: Failed to add fd to epoll list\n");
        close(proxy_server);
        close(epoll_fd);
        return;
    }

    int n = 0;
    struct epoll_event ev_list[MAX_EVENT];
    while (true) {
        if ((n = epoll_wait(epoll_fd, ev_list, MAX_EVENT, -1)) == SP_ERR) {
            fprintf(stderr, "Error: Failed to wait for epoll events\n");
            close(proxy_server);
            close(epoll_fd);
            return;
        }

        for (int i = 0; i < n; i++) {
            if (ev_list[i].data.fd == proxy_server) {
                fprintf(stderr, "Info: Accepting new incoming connection\n");
                Conn *conn = calloc(1, sizeof(Conn));
                if (conn == nullptr) {
                    fprintf(stderr, "Error: Failed to allocate memory\n");
                    return;
                }

                if ((conn->client = calloc(1, sizeof(Client) + BUFFER_SIZE + 1)) == nullptr) {
                    fprintf(stderr, "Error: Failed to allocate memory\n");
                    close(proxy_server);
                    free(conn);
                    return;
                }

                if ((conn->client->fd = accept(proxy_server, nullptr, nullptr)) == SP_ERR) {
                    fprintf(stderr, "Error: Failed to accept connection: %s\n", strerror(errno));
                    close_client(conn->client);
                    free(conn);
                    continue;
                }

                if (!set_nonblocking(conn->client->fd)) return;
                // if ((conn->server = calloc(1, sizeof(Client) + BUFFER_SIZE + 1)) == nullptr) {
                //     fprintf(stderr, "Error: Failed to allocate memory\n");
                //     close(proxy_server);
                //     free_client(conn->client);
                //     free(conn);
                //     return;
                // }

                ev.data.ptr = conn;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn->client->fd, &ev) == SP_ERR) {
                    fprintf(stderr, "Error: Failed to add fd to epoll list\n");
                    close(proxy_server);
                    close(epoll_fd);
                    return;
                }

            } else {
                Conn *conn = ev_list[i].data.ptr;
                connection_handler(conn);
            }
        }
    }
}
