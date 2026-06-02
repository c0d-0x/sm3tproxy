#include <stdint.h>
#include <stdio.h>

#include "args.h"
#include "sm3t_conf.h"
#include "sm3t_core.h"
#include "sm3t_proxy.h"

void print_help(Args *a, const char *program);

// TODO: Drop unnecessary CAPS
int main(int argc, char *argv[]) {
    Args cmd_arg = {};
    option_help(&cmd_arg, print_help);
    option_version(&cmd_arg, "sm3tproxy-" SM3T_VERSION);
    const long *port = option_long(&cmd_arg, "port", "Specify a local port to run proxy", .short_name = 'p',
                                   .default_value = SM3T_DEFAULT_PORT);
    const bool *debug = option_flag(&cmd_arg, "debug", "Show debug info", .short_name = 'd');
    const size_t *mode = option_enum(&cmd_arg, "mode", "Specify proxy server mode",
                                     ((const char *[]){"TTCP", "TCP", "SOCKS5", NULL}), .short_name = 'm');
    char **positional_args;
    parse_args(&cmd_arg, argc, argv, &positional_args);

    if (!(*port <= ((1 << 16) - 1))) {
        free_args(&cmd_arg);
        SM3T__FATAL("Invalid port number");
    }

    sm3t_conf_t *conf = &(sm3t_conf_t){
        .mode = *mode,
        .tcp.listen.port = (uint16_t) *port,
        .ctx_vtable[SM3T__MODE_TRANSPARENT] = sm3t__tcp_ctx_handler,
        .ctx_vtable[SM3T__MODE_FORWARD] = sm3t__tcp_ctx_handler,
        .ctx_vtable[SM3T__MODE_SOCKS5] = sm3t__tcp_echo,
        .tcp.telemetry.enable = *debug,
        .tcp.forward.upstreams
        = &(sm3t_upstreams_t){.name = "localhost", .ver = IPV4, .address = "127.0.0.1", .port = 3000},
    };

    sm3t__run_server(conf);
    free_args(&cmd_arg);
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
