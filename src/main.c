#include <stdio.h>
#include <stdlib.h>

#include "cli.h"
#include "proxy.h"

int main(int argc, char *argv[]) {
    printf("SM3TPROXY ON PORT: %s\n", DEFAULT_PORT);
    run_server(nullptr);
    return EXIT_SUCCESS;
}
