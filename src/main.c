#include <stdio.h>
#include <stdlib.h>

#include "cli.h"
#include "proxy.h"

// TODO: Drop unnecessary CAPS
int main(int argc, char *argv[]) {
    printf("SM3TPROXY ON PORT: %s\n", DEFAULT_PORT);
    sm3t__run_server(nullptr);
    return EXIT_SUCCESS;
}
