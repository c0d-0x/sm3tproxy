#include <stdio.h>
#include <stdlib.h>

#include <args.h>
#include <cmd.h>
#include <proxy.h>

// TODO: Drop unnecessary CAPS
int main(int argc, char *argv[]) {
    Args cmd_arg = {};
    option_version(&cmd_arg, "sm3tproxy-" VERSION);
    const char **port = option_string(&cmd_arg, "port", "Specify a local port to run proxy", .short_name = 'p',
                                      .default_value = DEFAULT_PORT);
    char **positional_args;
    parse_args(&cmd_arg, argc, argv, &positional_args);
    printf("SM3TPROXY ON PORT: %s\n", *port);
    sm3t__run_server((char *) *port);
    return EXIT_SUCCESS;
}
