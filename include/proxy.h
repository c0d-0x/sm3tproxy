#ifndef PROXY_H
#define PROXY_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <netinet/in.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>

#ifndef DEFAULT_PORT
#define DEFAULT_PORT "8080"
#endif

#define BACKLOG 10
#define BUFFER_SIZE 1024

#define MAX_EVENT 5

typedef struct {
    int n;
    int sock;
    char buffer[];
} Node;

typedef enum {
    CLIENT,
    SERVER,
} Active;

typedef struct {
    int server_port;
    int client_port;
    char server_ip[INET6_ADDRSTRLEN];
    char client_ip[INET6_ADDRSTRLEN];
} PeerInfo;

typedef struct {
    uint16_t refcount;
    Node *client;
    Node *server;
    PeerInfo info;
} Conn;

typedef struct Context {
    Active active;
    uint32_t events;
    Conn *conn;
    struct Context *peer;
} Context;

typedef enum {
    SM3T__OK = 0,
    SM3T__ERR = -1,
    SM3T__ERR_TIMEOUT = -2,
    SM3T__ERR_PROTOCOL = -3
} Status;

void sm3t__run_server(char *port);
bool sm3t__set_nonblocking(int fd);
#endif  // !PROXY_H
