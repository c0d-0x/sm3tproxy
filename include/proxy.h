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

typedef struct {
    int fd;
    char buffer[];
} Client;

typedef enum {
    SP_OK = 0,
    SP_ERR = -1,
    SP_ERR_TIMEOUT = -2,
    SP_ERR_PROTOCOL = -3
} SP_STATUS;

static int init_server(char *port);
void run_server(char *port);
void *connection_handler(void *client);
#endif  // !PROXY_H
