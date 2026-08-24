
#include <assert.h>
#include <lauxlib.h>
#include <lua.h>
#include <unistd.h>

#include "logger.h"
#include "sm3t_proxy.h"

static int l_remote_addr(lua_State *L) {
    sm3t_context_t *ctx = lua_touserdata(L, 1);
    if (ctx == NULL) {
        log_warn("NULL ctx");
        lua_pushnil(L);
        return 1;
    }

    lua_pushstring(L, ctx->meta.addr);
    return 1;
}

static int l_remote_port(lua_State *L) {
    sm3t_context_t *ctx = lua_touserdata(L, 1);
    if (ctx == NULL) {
        log_warn("NULL ctx");
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, ctx->meta.port);
    return 1;
}

static int l_log(lua_State *L) {
    const char *msg = luaL_checkstring(L, 1);
    log_info("[LUA] %s", msg);
    return 0;
}

static int l_on_connect(lua_State *L) {
    // TODO: implementation
    return 0;
}

static int l_on_data(lua_State *L) {
    // TODO: implementation
    return 0;
}

static int l_on_disconnect(lua_State *L) {
    // TODO: implementation
    return 0;
}

static const luaL_Reg proxy_lib[] = {
    {"rhost",         l_remote_addr  },
    {"rport",         l_remote_port  },
    {"log",           l_log          },
    {"on_connect",    l_on_connect   },
    {"on_data",       l_on_data      },
    {"on_disconnect", l_on_disconnect},
    {NULL,            NULL           }
};

void sm3t__register_api(lua_State *L) {
    luaL_newlib(L, proxy_lib);
    lua_setglobal(L, "proxy");
}
