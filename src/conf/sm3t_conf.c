#include "sm3t_conf.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
#include <stdio.h>

#include "logger.h"

static void sm3t__sandbox(lua_State *L) {
    static const char *blocked[] = {"dofile", "loadfile", "load", "require", NULL};

    for (int i = 0; blocked[i]; i++) {
        lua_pushnil(L);
        lua_setglobal(L, blocked[i]);
    }

    lua_getglobal(L, "os");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        lua_setfield(L, -2, "execute");
        lua_pushnil(L);
        lua_setfield(L, -2, "exit");
    }

    lua_pop(L, 1);
    lua_getglobal(L, "io");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        lua_setfield(L, -2, "popen");
    }

    lua_pop(L, 1);
}

bool sm3t__reload_conf(lua_State **L, const char *path) {
    sm3t__cleanup_conf(L);
    *L = sm3t__init_conf(path);
    return *L != NULL;
}

void sm3t__cleanup_conf(lua_State **L) {
    if (L != NULL && *L != NULL) {
        lua_close(*L);
        *L = NULL;
    }
}

lua_State *sm3t__init_conf(const char *path) {
    lua_State *L = NULL;
    if ((L = sm3t__load_conf(path)) == NULL) return NULL;

    sm3t__sandbox(L);
    if (lua_pcall(L, 0, LUA_MULTRET, 0) != LUA_OK) {
        log_error("[LUA] Failed to execute conf: %s", lua_tostring(L, -1));
        lua_close(L);
        return NULL;
    }

    if (!sm3t__parse_conf(L)) {
        lua_close(L);
        return NULL;
    }

    sm3t__register_api(L);
    return L;
}
