#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// CONFIG TYPES
typedef bool (*sm3t_ctx_handler_t)(void *, int);

typedef enum {
    SM3T__HTTP_PROXY,
    SM3T__HTTP_SERVER,
    SM3T__CUSTOM_SERVER,
    SM3T__TRANSPARENT_TCP_PROXY,
} sm3t_server_mode_t;

typedef struct {
    sm3t_server_mode_t mode;
    char *port;
    bool debug;
    sm3t_ctx_handler_t handler;
    void *lua_hook;  // TODO: properly define
} sm3t_conf_t;

sm3t_conf_t *sm3t__parse_conf(char *path);
sm3t_conf_t *sm3t__reload_conf(char *path);
sm3t_conf_t *sm3t__dump_conf(char *path);

#endif  // !CONFIG_H
