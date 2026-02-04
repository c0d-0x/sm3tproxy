#ifndef CORE_H
#define CORE_H

#include <stdlib.h>

#define VERSION "v1.0.0"

#ifndef SM3T_VEC_MAX
#define SM3T_VEC_MAX 16
#endif  // !SM3T_VEC_MAX

#define SM3T__FATAL(...)              \
    do {                              \
        fprintf(stderr, "ERROR: ");   \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n");        \
        exit(EXIT_FAILURE);           \
    } while (0)

#define SM3T__OUT_OF_MEMORY() SM3T__FATAL("Out of memory")

typedef struct sm3t_vec {
    size_t size;
    size_t capacity;
    void *data[];
} sm3t_vec_t;

void sm3t__destroy_vec(sm3t_vec_t *vec);
void sm3t__cleanup_vec(sm3t_vec_t *vec, void (*cleanup_callback)(void *data));
bool sm3t__vec_append(sm3t_vec_t **vec, void *data);
#endif  // !CORE_H
