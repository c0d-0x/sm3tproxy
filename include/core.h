#ifndef CORE_H
#define CORE_H

#include <stddef.h>
#include <stdint.h>
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

typedef struct {
    char *data;
    size_t len;
} sm3t_view_t;

void sm3t__destroy_vec(sm3t_vec_t *vec);
void sm3t__cleanup_vec(sm3t_vec_t *vec, void (*cleanup_callback)(void *data));
bool sm3t__vec_append(sm3t_vec_t **vec, void *data);

sm3t_view_t sm3t_view(const char *data, size_t len);
sm3t_view_t sm3t_view_cstr(const char *str);  // NULL terminated str

sm3t_view_t sm3t_view_slice(sm3t_view_t *view, size_t offset, size_t len);
sm3t_view_t sm3t_view_skip(sm3t_view_t *view, size_t n);

bool sm3t_view_starts_with(sm3t_view_t *view, sm3t_view_t *prefix);
bool sm3t_view_eq(sm3t_view_t *viewa, sm3t_view_t *viewb);

bool sm3t_view_empty(sm3t_view_t *view);
#endif  // !CORE_H
