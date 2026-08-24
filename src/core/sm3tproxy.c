#include <lua.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "args.h"
#include "sm3t_conf.h"
#include "sm3t_core.h"
#include "sm3t_proxy.h"

void print_help(Args *a, const char *program);

// TODO: Drop unnecessary CAPS
int main(int argc, char *argv[]) {
    Args cmd_arg = {};
    sm3t_value_t in_lua = {};
    sm3t_value_t out_lua = {};
    lua_State *L = NULL;
    option_help(&cmd_arg, print_help);
    option_version(&cmd_arg, "sm3tproxy-" SM3T_VERSION);
    const long *port = option_long(&cmd_arg, "port", "Specify a local port to run proxy", .short_name = 'p',
                                   .default_value = SM3T_DEFAULT_PORT);
    const bool *debug = option_flag(&cmd_arg, "debug", "Show debug info", .short_name = 'd');
    const size_t *mode = option_enum(&cmd_arg, "mode", "Specify proxy server mode",
                                     ((const char *[]){"TTCP", "TCP", "SOCKS5", NULL}), .short_name = 'm');
    char **positional_args;
    parse_args(&cmd_arg, argc, argv, &positional_args);

    if ((L = sm3t__init_conf("examples/conf.lua")) == NULL) {
        return EXIT_FAILURE;
    }

    if (*port > INT16_MAX || *port <= 0) {
        free_args(&cmd_arg);
        SM3T__FATAL("Invalid port number");
    }

    in_lua._int = (int64_t) *port;
    sm3t__set_conf_value(L, "tcp.listen.port", SM3T_CINT, &in_lua);

    memset(&in_lua, '\0', sizeof(sm3t_value_t));
    in_lua._bool = *debug;
    sm3t__set_conf_value(L, "tcp.telemetry.enable", SM3T_CBOOL, &in_lua);

    memset(&in_lua, '\0', sizeof(sm3t_value_t));
    in_lua._int = *mode;
    sm3t__set_conf_value(L, "tcp.listen.mode", SM3T_CINT, &in_lua);

    sm3t__run_server(L);
    free_args(&cmd_arg);
    lua_close(L);
    return EXIT_SUCCESS;
}

void print_help(Args *a, const char *program) {
    fprintf(stdout, "%s - Is an experimental programmable tcp proxy server\n", program);
    fprintf(stdout, "\n");
    fprintf(stdout, "Usage:\n");
    fprintf(stdout, "  %s [options]\n", program);
    fprintf(stdout, "\n");
    print_options(a, stdout);
    fprintf(stdout, "   Mode:\n");
    fprintf(stdout, "        TTCP   - Transparent proxy mode\n");
    fprintf(stdout, "        TCP    - Forward proxy mode\n");
    fprintf(stdout, "        SOCKS5 - SOCKS5 proxy mode\n");
}
