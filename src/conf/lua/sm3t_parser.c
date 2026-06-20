#include <lauxlib.h>
#include <lua.h>
#include <luajit-2.1/lauxlib.h>
#include <lualib.h>
#include <stdio.h>

#include "logger.h"
#include "sm3t_conf.h"

lua_State *sm3t__parse_conf(char *path) {
    lua_State *L = luaL_newstate();
    if (L == NULL) {
        log_error("Failed to initiate  a lua state");
        return NULL;
    }

    luaL_loadfile(L, path);
    return L;
}
