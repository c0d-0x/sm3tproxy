#ifndef CONF_H
#define CONF_H
#include "sm3t_conf.h"

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

bool sm3t__reload_conf(lua_State *L) { return NULL; }
sm3t_conf_t *sm3t_cleanup_conf(lua_State *L) { return NULL; }

#endif /* ifndef CONF_H */
