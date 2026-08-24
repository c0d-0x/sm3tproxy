#include <lauxlib.h>
#include <lua.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "logger.h"
#include "sm3t_conf.h"
#include "sm3t_core.h"

typedef bool(*sm3t_validator_t);

typedef struct conf_node {
    const char *_key;
    sm3t_type_t _type;
    bool _required;
    bool (*value_validator)(sm3t_value_t *, sm3t_type_t);
    const struct conf_node *_child;
} sm3t_conf_node_t;

#define SM3T__NEW_NODE(key, type, ...) {._key = key, ._type = type, __VA_ARGS__}
#define SM3T__TERMINATING_NODE {NULL, 0, false, NULL, NULL}

lua_State *sm3t__load_conf(const char *path) {
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        log_error("[LUA] Failed to initialize a lua state");
        return NULL;
    }

    if (luaL_loadfile(L, path) != LUA_OK) {
        log_error("[LUA] Failed to compile conf: %s", lua_tostring(L, -1));
        lua_close(L);
        return NULL;
    }

    return L;
}

static const char *sm3t__conf_type_name(sm3t_type_t type) {
    switch (type) {
        case SM3T_CINT:      return "integer";
        case SM3T_CFLOAT:    return "float";
        case SM3T_CBOOL:     return "boolean";
        case SM3T_CSTRING:   return "string";
        case SM3T_CTABLE:    return "table";
        case SM3T_CFUNCTION: return "function";
        default:             return "unknown";
    }
}

const sm3t_conf_node_t log_out_tab[] = {
    SM3T__NEW_NODE("file", SM3T_CBOOL),
    SM3T__NEW_NODE("stdout", SM3T_CBOOL),
    SM3T__NEW_NODE("syslog", SM3T_CBOOL),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t logging_tab[] = {
    SM3T__NEW_NODE("level", SM3T_CINT),
    SM3T__NEW_NODE("format", SM3T_CINT),
    SM3T__NEW_NODE("file_path", SM3T_CSTRING),
    SM3T__NEW_NODE("out", SM3T_CTABLE, ._child = log_out_tab),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t sys_tab[] = {
    SM3T__NEW_NODE("name", SM3T_CSTRING),
    SM3T__NEW_NODE("user", SM3T_CSTRING),
    SM3T__NEW_NODE("group", SM3T_CSTRING),
    SM3T__NEW_NODE("chroot", SM3T_CSTRING),
    SM3T__NEW_NODE("logging", SM3T_CTABLE, ._child = logging_tab),
    SM3T__TERMINATING_NODE,
};

bool verify_port(sm3t_value_t *port, sm3t_type_t type) {
    if (type != SM3T_CINT) return false;
    if (port->_int > UINT16_MAX || port->_int <= 0) return false;

    return true;
}

bool verify_mode(sm3t_value_t *mode, sm3t_type_t type) {
    if (mode->_int >= SM3T__MODE_COUNT || mode->_int < 0) return false;
    return true;
}

const sm3t_conf_node_t listen_tab[] = {
    SM3T__NEW_NODE("mode", SM3T_CINT, .value_validator = verify_mode),
    SM3T__NEW_NODE("port", SM3T_CINT, .value_validator = verify_port),
    SM3T__NEW_NODE("address", SM3T_CSTRING),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t orig_dst_tab[] = {
    SM3T__NEW_NODE("required", SM3T_CBOOL),
    SM3T__NEW_NODE("optional", SM3T_CBOOL),
    SM3T__NEW_NODE("ignored", SM3T_CBOOL),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t failure_policy_tab[] = {
    SM3T__NEW_NODE("drop", SM3T_CBOOL),
    SM3T__NEW_NODE("reset", SM3T_CBOOL),
    SM3T__NEW_NODE("bypass", SM3T_CBOOL),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t forward_tab[] = {
    SM3T__NEW_NODE("upstreams", SM3T_CTABLE),
    SM3T__NEW_NODE("upstreams_count", SM3T_CINT),
    SM3T__NEW_NODE("failure_policy", SM3T_CTABLE, ._child = failure_policy_tab),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t limit_tab[] = {
    SM3T__NEW_NODE("max_total", SM3T_CINT),
    SM3T__NEW_NODE("max_per_src", SM3T_CINT),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t timeout_tab[] = {
    SM3T__NEW_NODE("connect_ms", SM3T_CINT),
    SM3T__NEW_NODE("idle_ms", SM3T_CINT),
    SM3T__NEW_NODE("lifetime_ms", SM3T_CINT),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t retries_tab[] = {
    SM3T__NEW_NODE("enable", SM3T_CBOOL),
    SM3T__NEW_NODE("max", SM3T_CINT),
    SM3T__NEW_NODE("backoff_ms", SM3T_CINT),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t conn_tab[] = {
    SM3T__NEW_NODE("limit", SM3T_CTABLE, ._child = limit_tab),
    SM3T__NEW_NODE("timeout", SM3T_CTABLE, ._child = timeout_tab),
    SM3T__NEW_NODE("retries", SM3T_CTABLE, ._child = retries_tab),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t flow_control_tab[] = {
    SM3T__NEW_NODE("backpressure_stall", SM3T_CBOOL),
    SM3T__NEW_NODE("backpressure_close", SM3T_CBOOL),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t keepalive_tab[] = {
    SM3T__NEW_NODE("enable", SM3T_CBOOL),
    SM3T__NEW_NODE("idle_ms", SM3T_CINT),
    SM3T__NEW_NODE("interval_ms", SM3T_CINT),
    SM3T__NEW_NODE("count", SM3T_CINT),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t transport_tab[] = {
    SM3T__NEW_NODE("keepalive", SM3T_CTABLE, ._child = keepalive_tab),
    SM3T__NEW_NODE("nodelay", SM3T_CBOOL),
    SM3T__NEW_NODE("reset_on_violation", SM3T_CBOOL),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t hooks_tab[] = {
    SM3T__NEW_NODE("on_connect", SM3T_CFUNCTION),
    SM3T__NEW_NODE("on_data", SM3T_CFUNCTION),
    SM3T__NEW_NODE("on_disconnect", SM3T_CFUNCTION),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t metrics_tab[] = {
    SM3T__NEW_NODE("per_src", SM3T_CBOOL),
    SM3T__NEW_NODE("per_dst", SM3T_CBOOL),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t log_tab[] = {
    SM3T__NEW_NODE("src_ip", SM3T_CBOOL),
    SM3T__NEW_NODE("src_port", SM3T_CBOOL),
    SM3T__NEW_NODE("dst_ip", SM3T_CBOOL),
    SM3T__NEW_NODE("dst_port", SM3T_CBOOL),
    SM3T__NEW_NODE("bytes_in", SM3T_CBOOL),
    SM3T__NEW_NODE("bytes_out", SM3T_CBOOL),
    SM3T__NEW_NODE("duration", SM3T_CBOOL),
    SM3T__NEW_NODE("err", SM3T_CBOOL),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t telemetry_tab[] = {
    SM3T__NEW_NODE("enable", SM3T_CBOOL),
    SM3T__NEW_NODE("metrics", SM3T_CTABLE, ._child = metrics_tab),
    SM3T__NEW_NODE("log", SM3T_CTABLE, ._child = log_tab),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t tcp_tab[] = {
    SM3T__NEW_NODE("listen", SM3T_CTABLE, ._child = listen_tab),
    SM3T__NEW_NODE("orig_dst", SM3T_CTABLE, ._child = orig_dst_tab),
    SM3T__NEW_NODE("forward", SM3T_CTABLE, ._child = forward_tab),
    SM3T__NEW_NODE("conn", SM3T_CTABLE, ._child = conn_tab),
    SM3T__NEW_NODE("flow_control", SM3T_CTABLE, ._child = flow_control_tab),
    SM3T__NEW_NODE("transport", SM3T_CTABLE, ._child = transport_tab),
    SM3T__NEW_NODE("telemetry", SM3T_CTABLE, ._child = telemetry_tab),
    SM3T__TERMINATING_NODE,
};

const sm3t_conf_node_t root_tab[] = {
    SM3T__NEW_NODE("sys", SM3T_CTABLE, ._child = sys_tab),
    SM3T__NEW_NODE("tcp", SM3T_CTABLE, ._child = tcp_tab),
    SM3T__NEW_NODE("hooks", SM3T_CTABLE, ._child = hooks_tab),
    SM3T__TERMINATING_NODE,
};

#define tab_PATH_MAX 256

static bool validate_tab_node(lua_State *L, int index, const sm3t_conf_node_t *tab, const char *parent_path) {
    if (!lua_istable(L, index)) return false;

    int abs_idx = index < 0 ? lua_gettop(L) + index + 1 : index;
    bool all_valid = true;

    for (int i = 0; tab[i]._key != NULL; i++) {
        const sm3t_conf_node_t *node = &tab[i];
        lua_getfield(L, abs_idx, node->_key);

        char field_path[tab_PATH_MAX];
        if (parent_path[0] != '\0') snprintf(field_path, sizeof(field_path), "%s.%s", parent_path, node->_key);
        else snprintf(field_path, sizeof(field_path), "%s", node->_key);

        bool valid = true;
        switch (node->_type) {
            case SM3T_CINT:
            case SM3T_CFLOAT:
                if (!lua_isnumber(L, -1)) {
                    lua_pushnumber(L, 0);
                    if (node->_required) valid = false;
                    break;
                };

                if (node->value_validator != NULL) {
                    sm3t_value_t value = (node->_type == SM3T_CINT)
                                             ? (sm3t_value_t){._int = (int64_t) lua_tonumber(L, -1)}
                                             : (sm3t_value_t){._float = (float) lua_tonumber(L, -1)};
                    if (!node->value_validator(&value, node->_type)) valid = false;
                }
                break;
            case SM3T_CBOOL:
                if (!lua_isboolean(L, -1)) valid = false;
                break;
            case SM3T_CSTRING:
                if (!lua_isstring(L, -1) && node->_required) {
                    lua_pushnil(L);
                    if (node->_required) valid = false;
                    break;
                };

                if (node->value_validator != NULL) {
                    sm3t_value_t value = {._string = (char *) lua_tostring(L, -1)};
                    if (!node->value_validator(&value, SM3T_CSTRING)) valid = false;
                }
                break;
            case SM3T_CFUNCTION:
                if (!lua_isfunction(L, -1)) valid = false;
                break;
            case SM3T_CTABLE:
                if (!lua_istable(L, -1)) valid = false;
                else if (node->_child) valid = validate_tab_node(L, -1, node->_child, field_path);

                break;
            default: valid = false;
        }

        if (!valid) {
            log_error("[LUA] Failed to verify conf: field '%s' is missing or has incorrect type (expected %s)",
                      field_path, sm3t__conf_type_name(node->_type));
            all_valid = false;
        }

        lua_pop(L, 1);
    }

    return all_valid;
}

bool sm3t__parse_conf(lua_State *L) {
    if (!validate_tab_node(L, -1, root_tab, "")) {
        return false;
    }

    return true;
}

// Strings are allocated on the heap and the caller should cleanup
bool sm3t__lua_ctype(lua_State *L, const char *field, sm3t_type_t type, sm3t_value_t *out) {
    switch (type) {
        case SM3T_CSTRING:
            if (!lua_isstring(L, -1)) {
                log_error("[LUA] Field: %s, requires a valid string: %s", field, lua_tostring(L, -1));
                return false;
            }

            lua_Integer len = luaL_len(L, -1);
            if (len <= 0) {
                log_error("[LUA] Field: %s, failed to get field length: %s", field, lua_tostring(L, -1));
                return false;
            }

            if ((out->_string = malloc(sizeof(char) * len + 1)) == NULL) SM3T__OUT_OF_MEMORY();
            const char *luastr = lua_tostring(L, -1);
            if (luastr == NULL) {
                log_error("[LUA] Field: %s, lua_tostring returned NULL", field);
                free(out->_string);
                out->_string = NULL;
                return false;
            }

            memcpy(out->_string, luastr, len);
            out->_string[len] = '\0';
            lua_pop(L, 1);
            break;

        case SM3T_CBOOL:
            if (!lua_isboolean(L, -1)) {
                log_error("[LUA] Field %s, requires a boolean value: %s", field, lua_tostring(L, -1));
                return false;
            }

            out->_bool = lua_toboolean(L, -1);
            lua_pop(L, 1);
            break;

        default:
            if (type != SM3T_CINT && type != SM3T_CFLOAT) return false;

            if (!lua_isnumber(L, -1)) {
                log_error("[LUA] Field %s, requires a valid number: %s", field, lua_tostring(L, -1));
                return false;
            }

            lua_Number num = lua_tonumber(L, -1);
            if (type == SM3T_CINT) out->_int = (int64_t) num;
            if (type == SM3T_CFLOAT) out->_float = (double) num;
            lua_pop(L, 1);
    }

    return true;
}
