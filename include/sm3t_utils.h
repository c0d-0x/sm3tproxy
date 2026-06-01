#ifndef SM3T_UTIL_H
#define SM3T_UTIL_H

#include <stdint.h>
#include <sys/types.h>

#define SM3T_VEC_MAX 8
#define SM3T_NOT_FOUND (-1)

typedef struct sm3t_vec {
    size_t size;
    size_t capacity;
    void *data[];
} sm3t_vec_t;

typedef struct {
    char *data;
    int64_t len;
} sm3t_view_t;

// Vector
void sm3t__destroy_vec(sm3t_vec_t *vec);
void sm3t__cleanup_vec(sm3t_vec_t *vec, void (*cleanup_callback)(void *data));
bool sm3t__vec_append(sm3t_vec_t **vec, void *data);

// String processing utilities
sm3t_view_t sm3t_view(const char *data, int64_t len);
sm3t_view_t sm3t_view_cstr(const char *str);

sm3t_view_t sm3t_view_slice(sm3t_view_t *view, int64_t offset, int64_t len);
sm3t_view_t sm3t_view_skip(sm3t_view_t *view, int64_t n);

bool sm3t_view_starts_with(sm3t_view_t *view, sm3t_view_t *prefix);
bool sm3t_view_eq(sm3t_view_t *viewa, sm3t_view_t *viewb);
bool sm3t_view_empty(sm3t_view_t *view);

bool sm3t_view_ends_with(sm3t_view_t *view, sm3t_view_t *suffix);
int64_t sm3t_view_find(sm3t_view_t *view, sm3t_view_t *needle, int64_t start_offset);
int64_t sm3t_view_find_char(sm3t_view_t *view, char c, int64_t start_offset);
bool sm3t_view_split(sm3t_view_t *view, char delim, sm3t_view_t *left, sm3t_view_t *right);
sm3t_view_t sm3t_view_trim(sm3t_view_t *view);
sm3t_view_t sm3t_view_trim_left(sm3t_view_t *view);
sm3t_view_t sm3t_view_trim_right(sm3t_view_t *view);
bool sm3t_view_eq_case(sm3t_view_t *viewa, sm3t_view_t *viewb);
bool sm3t_view_starts_with_case(sm3t_view_t *view, sm3t_view_t *prefix);
bool sm3t_view_to_u32(sm3t_view_t *view, uint32_t *out);
char *sm3t_view_to_cstr(sm3t_view_t *view);

#endif  // !SM3T_UTIL_H
