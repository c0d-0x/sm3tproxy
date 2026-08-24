#include <lauxlib.h>
#include <lua.h>
#include <sched.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "logger.h"
#include "sm3t_conf.h"
#include "sm3t_proxy.h"

sm3t_hook_status_t sm3t__hook_on_connect(lua_State *L, sm3t_context_t *client_ctx) {
    if (L == NULL) return SM3T_HOOK_PASS;

    if (!lua_istable(L, 1)) return SM3T_HOOK_PASS;

    lua_getfield(L, 1, "hooks");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return SM3T_HOOK_PASS;
    }

    lua_getfield(L, -1, "on_connect");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return SM3T_HOOK_PASS;
    }

    lua_pushlightuserdata(L, client_ctx);

    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        log_error("on_connect: %s", lua_tostring(L, -1));
        lua_pop(L, 2);
        return SM3T_HOOK_PASS;
    }

    sm3t_hook_status_t result = lua_toboolean(L, -1) ? SM3T_HOOK_PASS : SM3T_HOOK_DROP;
    lua_pop(L, 2);
    return result;
}

void sm3t__hook_on_disconnect(lua_State *L, sm3t_context_t *ctx) {
    if (L == NULL) return;

    if (!lua_istable(L, 1)) return;

    lua_getfield(L, 1, "hooks");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "on_disconnect");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return;
    }

    lua_pushlightuserdata(L, ctx);

    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        log_error("on_disconnect: %s", lua_tostring(L, -1));
        lua_pop(L, 2);
        return;
    }

    lua_pop(L, 1);
}

sm3t_hook_status_t sm3t__hook_on_data(lua_State *L, sm3t_context_t *ctx, uint8_t **buf, ssize_t *len,
                                      sm3t_direction_t direction) {
    if (L == NULL) return SM3T_HOOK_PASS;
    if (!lua_istable(L, 1)) return SM3T_HOOK_PASS;

    lua_getfield(L, 1, "hooks");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return SM3T_HOOK_PASS;
    }

    lua_getfield(L, -1, "on_data");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return SM3T_HOOK_PASS;
    }

    lua_pushlightuserdata(L, ctx);
    lua_pushlstring(L, (const char *) *buf, (size_t) *len);
    lua_pushinteger(L, direction);

    if (lua_pcall(L, 3, 1, 0) != LUA_OK) {
        log_error("on_data: %s", lua_tostring(L, -1));
        lua_pop(L, 2);
        return SM3T_HOOK_PASS;
    }

    sm3t_hook_status_t result = SM3T_HOOK_DROP;
    if (lua_isstring(L, -1)) {
        size_t new_len = luaL_len(L, -1);

        if (new_len == 0) {
            log_warn("on_data: hook returned Zero-bytes - ignoring");
            result = SM3T_HOOK_PASS;
        }

        else if (new_len <= SM3T_BUFFER_SIZE) {
            const char *new_data = lua_tolstring(L, -1, &new_len);
            memcpy(*buf, new_data, new_len);
            *len = (ssize_t) new_len;
            buf[*len] = 0;
            result = SM3T_HOOK_PASS;
        } else log_warn("on_data: hook returned %zu bytes, exceeds buffer - ignoring", new_len);
    }

    lua_pop(L, 2);
    return result;
}
