
#include <args.h>
#include <core.h>
#include <proxy.h>
#include <stdint.h>

// TODO: Drop unnecessary CAPS
void print_help(Args *a, const char *program) {
    printf("%s - Is an experimental programmable proxy server\n", program);
    printf("\n");
    printf("Usage:\n");
    printf("  %s [options]\n", program);
    printf("\n");
    print_options(a, stdout);
}

int main(int argc, char *argv[]) {
    Args cmd_arg = {};
    option_help(&cmd_arg, print_help);
    option_version(&cmd_arg, "sm3tproxy-" VERSION);
    const long *port = option_long(&cmd_arg, "port", "Specify a local port to run proxy", .short_name = 'p',
                                   .default_value = DEFAULT_PORT);
    const bool *debug = option_flag(&cmd_arg, "debug", "Show debug info", .short_name = 'd');
    char **positional_args;
    parse_args(&cmd_arg, argc, argv, &positional_args);
    sm3t_conf_t *conf = &(sm3t_conf_t) {.mode = SM3T__TCP_PROXY,
                                        .tcp_proxy.listen.port = (uint16_t) *port,
                                        .tcp_proxy.interception.ctx_handler = sm3t__ttcp_ctx_handler,
                                        .global.logging.log_address = *debug};
    sm3t__run_server(conf);
    return EXIT_SUCCESS;
}
