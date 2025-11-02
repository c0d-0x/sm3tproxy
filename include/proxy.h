#ifndef PROXY_H
#define PROXY_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif /* ifndef _POSIX_C_SOURCE */

#include <sys/socket.h>
#include <sys/types.h>

#ifndef DEFAULT_PORT
#define DEFAULT_PORT "2000"
#endif  // !DEFAULT_PORT

#define BACKLOG 10
#define BUFFER_SIZE 1024

#define MAX_EVENT 5

typedef struct {
    int fd;
    int n;
    char buffer[];
} Client;

typedef struct {
    Client *client;
    Client *server;
} Conn;

typedef enum {
    SP_OK = 0,
    SP_ERR = -1,
    SP_ERR_TIMEOUT = -2,
    SP_ERR_PROTOCOL = -3
} SP_STATUS;

void run_server(char *port);
void connection_handler(Conn *conn);
bool set_nonblocking(int fd);
#endif  // !PROXY_H
