#ifndef CONFIG_H
#define CONFIG_H

// TODO: NOT needed entirely as the config will be handled with lua
#include <lua.h>
#include <stddef.h>
#include <stdint.h>
#include "sm3t_proxy.h"

typedef bool (*sm3t_ctx_handler_t)(lua_State *L, void *, int);

typedef enum : uint8_t {
    SM3T__MODE_TRANSPARENT,
    SM3T__MODE_FORWARD,
    SM3T__MODE_SOCKS5,
    SM3T__MODE_COUNT
} sm3t_server_mode_t;

typedef enum : uint8_t {
    SM3T__IPV4,
    SM3T__IPV6
} sm3t_addr_var_t;

typedef enum : uint8_t {
    SM3T__LOG_ERROR,
    SM3T__LOG_WARN,
    SM3T__LOG_INFO,
    SM3T__LOG_DEBUG
} sm3t_log_level_t;

typedef enum : uint8_t {
    SM3T__LOG_STRUCTURED,
    SM3T__LOG_PLAIN
} sm3t_log_fmt_t;

typedef enum : uint8_t {
    SM3T__LOG_STDOUT,
    SM3T__LOG_FILE,
    SM3T__LOG_SYSLOG
} sm3t_log_out_t;

typedef enum : uint8_t {
    SM3T__ORIG_REQUIRED,
    SM3T__ORIG_OPTIONAL,
    SM3T__ORIG_IGNORED
} sm3t_orig_dst_policy_t;

typedef enum : uint8_t {
    SM3T__FORWARD_ORIG,
    SM3T__FORWARD_FIXED
} sm3t_forwarding_mode_t;

typedef enum : uint8_t {
    SM3T__FAILURE_RESET,
    SM3T__FAILURE_DROP,
    SM3T__FAILURE_BYPASS
} sm3t_failure_policy_t;

typedef enum : uint8_t {
    SM3T__BACKPRESSURE_CLOSE,
    SM3T__BACKPRESSURE_STALL
} sm3t_backpressure_policy_t;

typedef struct {
    char *name;
    sm3t_addr_var_t ver;
    char *address;
    uint16_t port;
    uint32_t weight;
} sm3t_upstream_t;

typedef enum {
    SM3T_CINT,
    SM3T_CBOOL,
    SM3T_CFLOAT,
    SM3T_CSTRING,
    SM3T_CTABLE,
    SM3T_CFUNCTION,
} sm3t_type_t;

typedef struct {
    union {
        int64_t _int;
        double _float;
        bool _bool;
        char *_string;
    } as;
} sm3t_value_t;

typedef enum {
    SM3T_HOOK_PASS,
    SM3T_HOOK_DROP
} sm3t_hook_status_t;

typedef enum {
    SM3T_CLI_UPS,  // client to upstream
    SM3T_UPT_CLI   // upstream to client
} sm3t_direction_t;

void sm3t__hook_on_disconnect(lua_State *L, sm3t_context_t *ctx);
sm3t_hook_status_t sm3t__hook_on_connect(lua_State *L, sm3t_context_t *client_ctx);
sm3t_hook_status_t sm3t__hook_on_data(lua_State *L, sm3t_context_t *ctx, uint8_t **buf, ssize_t *len,
                                      sm3t_direction_t dir);

void sm3t__register_api(lua_State *L);
lua_State *sm3t__init_conf(const char *path);
bool sm3t__reload_conf(lua_State **L, const char *path);
lua_State *sm3t__load_conf(const char *path);
bool sm3t__parse_conf(lua_State *l);
bool sm3t__lua_ctype(lua_State *L, const char *field, sm3t_type_t type, sm3t_value_t *out);
bool sm3t__get_conf_value(lua_State *L, const char *path, sm3t_type_t type, sm3t_value_t *out);
bool sm3t__set_conf_value(lua_State *L, const char *path, sm3t_type_t type, sm3t_value_t *in);
void sm3t__cleanup_conf(lua_State **L);
#endif  // !CONFIG_H
