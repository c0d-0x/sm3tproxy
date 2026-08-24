#include <sched.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "sm3t_core.h"
#include "sm3t_utils.h"

static inline char ascii_tolower(char cc) {
    if (cc >= 'A' && cc <= 'Z') return cc + ('a' - 'A');
    return cc;
}

static inline bool ascii_isspace(char cc) {
    return cc == ' ' || cc == '\t' || cc == '\r' || cc == '\n' || cc == '\v' || cc == '\f';
}

bool sm3t__view_empty(sm3t_view_t *view) { return (view == NULL || view->data == NULL || view->len == 0); }

sm3t_view_t sm3t__view(const char *data, int64_t len) {
    if (data == NULL) return (sm3t_view_t){.data = NULL, .len = 0};
    return (sm3t_view_t){.data = (char *) data, .len = len};
}

sm3t_view_t sm3t__view_cstr(const char *str) {
    if (str == NULL) return (sm3t_view_t){.data = NULL, .len = 0};
    return (sm3t_view_t){.data = (char *) str, .len = strlen(str)};
}

sm3t_view_t sm3t__view_slice(sm3t_view_t *view, int64_t offset, int64_t len) {
    if (view == NULL || view->data == NULL || offset >= view->len) return (sm3t_view_t){.data = NULL, .len = 0};

    int64_t rem = view->len - offset;
    if (len > rem) len = rem;
    return (sm3t_view_t){.data = view->data + offset, .len = len};
}

sm3t_view_t sm3t__view_skip(sm3t_view_t *view, int64_t n) {
    if (view == NULL || view->data == NULL || n >= view->len) return (sm3t_view_t){.data = NULL, .len = 0};
    return (sm3t_view_t){.data = view->data + n, .len = view->len - n};
}

bool sm3t__view_starts_with(sm3t_view_t *view, sm3t_view_t *prefix) {
    if (prefix == NULL || prefix->len == 0) return true;
    if (view == NULL || view->data == NULL || view->len < prefix->len || prefix->data == NULL) return false;

    return memcmp(view->data, prefix->data, prefix->len) == 0;
}

bool sm3t__view_eq(sm3t_view_t *viewa, sm3t_view_t *viewb) {
    bool empty_a = sm3t__view_empty(viewa);
    bool empty_b = sm3t__view_empty(viewb);

    if (empty_a && empty_b) return true;
    if (empty_a || empty_b) return false;
    if (viewa->len != viewb->len) return false;

    return memcmp(viewa->data, viewb->data, viewa->len) == 0;
}

bool sm3t__view_ends_with(sm3t_view_t *view, sm3t_view_t *suffix) {
    if (suffix == NULL || suffix->len == 0) return true;
    if (view == NULL || view->data == NULL || view->len < suffix->len || suffix->data == NULL) return false;

    int64_t offset = view->len - suffix->len;
    return memcmp(view->data + offset, suffix->data, suffix->len) == 0;
}

int64_t sm3t__view_find(sm3t_view_t *view, sm3t_view_t *dlm, int64_t start_offset) {
    if (view == NULL || view->data == NULL || dlm == NULL || dlm->data == NULL) return SM3T_NOT_FOUND;
    if (dlm->len == 0) return start_offset <= view->len ? start_offset : SM3T_NOT_FOUND;
    if (start_offset >= view->len || view->len - start_offset < dlm->len) return SM3T_NOT_FOUND;

    int64_t search_limit = view->len - dlm->len;
    for (int64_t i = start_offset; i <= search_limit; i++) {
        if (memcmp(view->data + i, dlm->data, dlm->len) == 0) return i;
    }

    return SM3T_NOT_FOUND;
}

int64_t sm3t__view_find_char(sm3t_view_t *view, char cc, int64_t start_offset) {
    if (view == NULL || view->data == NULL || start_offset >= view->len) return SM3T_NOT_FOUND;

    for (int64_t i = start_offset; i < view->len; i++) {
        if (view->data[i] == cc) return i;
    }

    return SM3T_NOT_FOUND;
}

bool sm3t__view_split(sm3t_view_t *view, char delim, sm3t_view_t *left, sm3t_view_t *right) {
    if (view == NULL || view->data == NULL || left == NULL || right == NULL) return false;

    int64_t idx = sm3t__view_find_char(view, delim, 0);
    if (idx == SM3T_NOT_FOUND) {
        *left = *view;
        *right = (sm3t_view_t){.data = NULL, .len = 0};
        return false;
    }

    *left = (sm3t_view_t){.data = view->data, .len = idx};
    if (idx + 1 >= view->len) *right = (sm3t_view_t){.data = NULL, .len = 0};
    else *right = (sm3t_view_t){.data = view->data + (idx + 1), .len = view->len - (idx + 1)};

    return true;
}

sm3t_view_t sm3t__view_trim(sm3t_view_t *view) {
    sm3t_view_t ltrimmed = sm3t__view_trim_left(view);
    return sm3t__view_trim_right(&ltrimmed);
}

sm3t_view_t sm3t__view_trim_left(sm3t_view_t *view) {
    if (view == NULL || view->data == NULL) return (sm3t_view_t){.data = NULL, .len = 0};

    int64_t start = 0;
    while (start < view->len && ascii_isspace(view->data[start++]));

    if (start >= view->len) return (sm3t_view_t){.data = NULL, .len = 0};
    return (sm3t_view_t){.data = view->data + start, .len = view->len - start};
}

sm3t_view_t sm3t__view_trim_right(sm3t_view_t *view) {
    if (view == NULL || view->data == NULL || view->len == 0) return (sm3t_view_t){.data = NULL, .len = 0};

    int64_t end = view->len;
    while (end > 0 && ascii_isspace(view->data[--end]));

    if (end == 0) return (sm3t_view_t){.data = NULL, .len = 0};
    return (sm3t_view_t){.data = view->data, .len = end};
}

bool sm3t__view_eq_case(sm3t_view_t *viewa, sm3t_view_t *viewb) {
    bool empty_a = sm3t__view_empty(viewa);
    bool empty_b = sm3t__view_empty(viewb);

    if (empty_a && empty_b) return true;
    if (empty_a || empty_b) return false;
    if (viewa->len != viewb->len) return false;
    for (int64_t i = 0; i < viewa->len; i++) {
        if (ascii_tolower(viewa->data[i]) != ascii_tolower(viewb->data[i])) return false;
    }

    return true;
}

bool sm3t__view_starts_with_case(sm3t_view_t *view, sm3t_view_t *prefix) {
    if (prefix == NULL || prefix->len == 0) return true;
    if (view == NULL || view->data == NULL || view->len < prefix->len || prefix->data == NULL) return false;

    for (int64_t i = 0; i < prefix->len; i++) {
        if (ascii_tolower(view->data[i]) != ascii_tolower(prefix->data[i])) return false;
    }

    return true;
}

bool sm3t__view_to_u32(sm3t_view_t *view, uint32_t *out) {
    if (view == NULL || view->data == NULL || view->len == 0 || out == NULL) return false;

    uint64_t val = 0;
    for (int64_t i = 0; i < view->len; i++) {
        char cc = view->data[i];
        if (cc < '0' || cc > '9') return false;

        val = val * 10 + (cc - '0');
        if (val > UINT32_MAX) return false;
    }

    *out = (uint32_t) val;
    return true;
}

char *sm3t__view_to_cstr(sm3t_view_t *view) {
    if (view == NULL || view->data == NULL || view->len == 0) return NULL;

    char *dest = malloc(view->len + 1);
    if (dest == NULL) SM3T__OUT_OF_MEMORY();
    memcpy(dest, view->data, view->len);
    dest[view->len] = '\0';
    return dest;
}
