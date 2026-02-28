#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "args.h"
#include "conf.h"
#include "core.h"
#include "proxy.h"

void print_help(Args *a, const char *program);

// TODO: Drop unnecessary CAPS
int main(int argc, char *argv[]) {
    // NOTE: Runs in either TCP transparent or plain mode.
    Args cmd_arg = {};
    option_help(&cmd_arg, print_help);
    option_version(&cmd_arg, "sm3tproxy-" SM3T_VERSION);
    const long *port = option_long(&cmd_arg, "port", "Specify a local port to run proxy", .short_name = 'p',
                                   .default_value = SM3T_DEFAULT_PORT);
    const bool *debug = option_flag(&cmd_arg, "debug", "Show debug info", .short_name = 'd');
    char **positional_args;
    parse_args(&cmd_arg, argc, argv, &positional_args);

    if (!(*port <= ((1 << 16) - 1))) SM3T__FATAL("Invalid port number");

    sm3t_conf_t *conf = &(sm3t_conf_t) {.mode = SM3T__MODE_TRANSPARENT,
                                        .tcp.listen.port = (uint16_t) *port,
                                        .ctx_vtable[SM3T__MODE_TRANSPARENT] = sm3t__ttcp_ctx_handler,
                                        .global.logging.log_address = *debug};
    sm3t__run_server(conf);
    return EXIT_SUCCESS;
}

void print_help(Args *a, const char *program) {
    fprintf(stderr, "%s - Is an experimental programmable proxy server\n", program);
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s [options]\n", program);
    fprintf(stderr, "\n");
    print_options(a, stdout);
}
