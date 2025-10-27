#include "proxy.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

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

    if (listen(sock_fd, BACKLOG) == SP_ERR) {
        fprintf(stderr, "Error: Failed to listen: %s\n", strerror(errno));
        freeaddrinfo(host);
        return SP_ERR;
    }

    freeaddrinfo(host);
    return sock_fd;
}

static void free_client(Client *client) {
    if (client != nullptr) {
        close(client->fd);
        free(client);
    }
}

void *connection_handler(void *arg) {
    assert(arg != nullptr);
    int n = 0;
    Client *client = (Client *) arg;

    fprintf(stderr, "Info: Received connection\n");
    char buf[] = "Welcome bruh!!!\n";
    send(client->fd, buf, sizeof(buf), 0);

    while (true) {
        if ((n = recv(client->fd, client->buffer, BUFFER_SIZE, 0)) == 0) {
            fprintf(stderr, "Info: connection closed\n");
            break;
        }

        client->buffer[n] = '\0';
        fprintf(stdout, "MSG: %s\n", client->buffer);
        if ((n = send(client->fd, client->buffer, n, 0)) == 0) {
            fprintf(stderr, "Info: connection closed\n");
            break;
        }
    }

    free_client(client);
    return nullptr;
}

void run_server(char *port) {
    if (port == nullptr) port = DEFAULT_PORT;

    int server = init_server(port);
    Client *client = nullptr;

    while (true) {
        if ((client = calloc(1, sizeof(Client) + BUFFER_SIZE + 1)) == nullptr) {
            fprintf(stderr, "Error: Failed to allocate memory\n");
            close(server);
            return;
        }

        if ((client->fd = accept(server, nullptr, nullptr)) == SP_ERR) {
            fprintf(stderr, "Error: Failed to accept connection: %s\n", strerror(errno));
            free(client);
            continue;
        }

        pthread_t thread;
        pthread_create(&thread, nullptr, connection_handler, (void *) client);
        pthread_detach(thread);
    }
}
