#include <lauxlib.h>
#include <lua.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "logger.h"
#include "sm3t_conf.h"

static bool sm3t__traverse_path(lua_State *L, const char *path) {
    char *path_copy = strdup(path);
    if (!path_copy) {
        log_error("[LUA] Out of memory");
        return false;
    }

    lua_pushvalue(L, 1);
    char *token = strtok(path_copy, ".");
    while (token != NULL) {
        if (!lua_istable(L, -1)) {
            log_error("[LUA] Path '%s' is invalid, intermediate node is not a table", path);
            lua_pop(L, 1);
            free(path_copy);
            return false;
        }

        lua_getfield(L, -1, token);
        lua_remove(L, -2);

        token = strtok(NULL, ".");
    }

    free(path_copy);
    return true;
}

bool sm3t__get_conf_value(lua_State *L, const char *path, sm3t_type_t type, sm3t_value_t *out) {
    if (!lua_istable(L, 1)) {
        log_error("[LUA] Conf table is not at index 1");
        return false;
    }

    if (!sm3t__traverse_path(L, path)) {
        return false;
    }

    bool ok = sm3t__lua_ctype(L, path, type, out);
    if (!ok) lua_pop(L, 1);

    return ok;
}

bool sm3t__set_conf_value(lua_State *L, const char *path, sm3t_type_t type, sm3t_value_t *in) {
    if (!lua_istable(L, 1)) {
        log_error("[LUA] Conf table is not at index 1");
        return false;
    }

    char *path_copy = strdup(path);
    if (!path_copy) {
        log_error("[LUA] Out of memory");
        return false;
    }

    char *last_dot = strrchr(path_copy, '.');
    char *key = path_copy;

    if (last_dot != NULL) {
        *last_dot = '\0';
        key = last_dot + 1;

        if (!sm3t__traverse_path(L, path_copy)) {
            free(path_copy);
            return false;
        }
    } else {
        lua_pushvalue(L, 1);
    }

    if (!lua_istable(L, -1)) {
        log_error("[LUA] Path parent node is not a table");
        lua_pop(L, 1);
        free(path_copy);
        return false;
    }

    switch (type) {
        case SM3T_CINT:    lua_pushinteger(L, in->_int); break;
        case SM3T_CFLOAT:  lua_pushnumber(L, in->_float); break;
        case SM3T_CBOOL:   lua_pushboolean(L, in->_bool); break;
        case SM3T_CSTRING: lua_pushstring(L, in->_string); break;
        default:
            log_error("[LUA] Unsupported type for setter at path '%s'", path);
            lua_pop(L, 1);
            free(path_copy);
            return false;
    }

    lua_setfield(L, -2, key);

    lua_pop(L, 1);
    free(path_copy);
    return true;
}
