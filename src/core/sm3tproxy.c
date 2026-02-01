
#include <args.h>
#include <core.h>
#include <proxy.h>

// TODO: Drop unnecessary CAPS

int main(int argc, char *argv[]) {
    Args cmd_arg = {};
    option_version(&cmd_arg, "sm3tproxy-" VERSION);
    const char **port = option_string(&cmd_arg, "port", "Specify a local port to run proxy", .short_name = 'p',
                                      .default_value = DEFAULT_PORT);
    bool debug = option_flag(&cmd_arg, "debug", "Show debug info", .short_name = 'd');
    char **positional_args;
    parse_args(&cmd_arg, argc, argv, &positional_args);
    sm3t_conf_t *conf = &(sm3t_conf_t) {
        .mode = SM3T__TRANSPARENT_TCP_PROXY, .port = (char *) *port, .handler = sm3t__ttcp_ctx_handler, .debug = debug};
    sm3t__run_server(conf);
    return EXIT_SUCCESS;
}
