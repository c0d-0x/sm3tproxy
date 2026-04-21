#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core.h"

sm3t_view_t sm3t__view(const void *data, size_t len) {
    sm3t_view_t view = (sm3t_view_t){.data = (void *) data, .len = len};
    return view;
}

sm3t_view_t sm3t__view_cstr(const char *data) {
    size_t len = sizeof(data);
    sm3t_view_t view = (sm3t_view_t){.data = (void *) data, .len = len};
    return view;
}

sm3t_view_t sm3t_view_slice(sm3t_view_t *view, size_t offset, size_t len) {
    char *data = (view != NULL) ? &view->data[offset] : NULL;
    return (sm3t_view_t){.data = data, .len = len};
}

sm3t_view_t sm3t_view_skip(sm3t_view_t *view, size_t n) {
    char *data = (view != NULL) ? &view->data[n] : NULL;
    return (sm3t_view_t){.data = data, .len = (view != NULL) ? view->len - n : 0};
}

bool sm3t_view_starts_with(sm3t_view_t *view, sm3t_view_t *prefix) {
    if (!sm3t_view_empty(view) && !sm3t_view_empty(prefix))
        if (strncmp(view->data, prefix->data, prefix->len) == 0) return true;
    return false;
}

bool sm3t_view_eq(sm3t_view_t *viewa, sm3t_view_t *viewb) {
    if (!sm3t_view_empty(viewa) && !sm3t_view_empty(viewb))
        if (strncmp(viewa->data, viewb->data, viewb->len) == 0) return true;
    return false;
}

bool sm3t_view_empty(sm3t_view_t *view) { return ((view != NULL) && (view->data == NULL || view->len == 0)); }
