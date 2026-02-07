#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "core.h"

bool sm3t__vec_append(sm3t_vec_t **vec, void *data) {
    if (data == NULL) return false;
    if (*vec == NULL) {
        if ((*vec = malloc(sizeof(sm3t_vec_t) + sizeof(data) * SM3T_VEC_MAX)) == NULL) SM3T__OUT_OF_MEMORY();

        (*vec)->capacity = SM3T_VEC_MAX;
        (*vec)->size = 0;
    } else if ((*vec)->size == (*vec)->capacity) {
        size_t capacity = (*vec)->capacity * 2;
        sm3t_vec_t *new_vec = realloc(*vec, sizeof(sm3t_vec_t) + sizeof(data) * capacity);
        if (new_vec == NULL) SM3T__OUT_OF_MEMORY();

        *vec = new_vec;
        (*vec)->capacity = capacity;
    }

    (*vec)->data[(*vec)->size++] = data;
    return true;
}

void sm3t__cleanup_vec(sm3t_vec_t *vec, void (*cleanup_callback)(void *data)) {
    if (vec == NULL) return;
    for (size_t i = 0; i < vec->size; i++) {
        cleanup_callback(vec->data[i]);
    }

    vec->size = 0;
}

void sm3t__destroy_vec(sm3t_vec_t *vec) { free(vec); }
