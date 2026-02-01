#ifndef CORE_H
#define CORE_H

#include <stdlib.h>
#define VERSION "v1.0.0"

#define SM3T__FATAL(...)              \
    do {                              \
        fprintf(stderr, "ERROR: ");   \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
        exit(EXIT_FAILURE);           \
    } while (0)

#define SM3T__OUT_OF_MEMORY() SM3T__FATAL("Out of memory")

#endif  // !CORE_H
