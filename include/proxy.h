#ifndef PROXY_H
#define PROXY_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <sys/socket.h>
#include <sys/types.h>

#ifndef DEFAULT_PORT
#define DEFAULT_PORT "8080"
#endif

#define BACKLOG 10
#define BUFFER_SIZE 1024

#define MAX_EVENT 5

typedef struct {
    int fd;
    int n;
    char buffer[];
} Client;

typedef enum {
    CLIENT,
    SERVER,
} Active;

typedef struct {
    Client *client;
    Client *server;
} Conn;

typedef struct {
    Active active;
    Conn *conn;
} Context;

typedef enum {
    SP_OK = 0,
    SP_ERR = -1,
    SP_ERR_TIMEOUT = -2,
    SP_ERR_PROTOCOL = -3
} Status;

void run_server(char *port);
void connection_handler(Conn *conn);
bool set_nonblocking(int fd);
#endif  // !PROXY_H
