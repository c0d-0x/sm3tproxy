#include <sched.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "sm3t_core.h"
#include "sm3t_utils.h"

bool sm3t__enqueue(sm3t_queue_t **queue, void *data) {
    if (data == NULL) return false;
    if (sm3t__queue_empty(*queue)) {
        if ((*queue = malloc(sizeof(sm3t_queue_t) + sizeof(data) * QUEUE_MAX)) == NULL) {
            return false;
        }

        (*queue)->capacity = QUEUE_MAX;
        (*queue)->size = 0;
        (*queue)->head = 0;
        (*queue)->tail = 0;
    }

    if (sm3t__queue_full(*queue)) {
        sm3t_queue_t *tmp = realloc(*queue, sizeof(queue) + sizeof(data) * (*queue)->capacity * 2);
        if (tmp == NULL) SM3T__OUT_OF_MEMORY();

        *queue = tmp;
        (*queue)->capacity *= 2;
    }

    (*queue)->data[(*queue)->tail] = data;
    (*queue)->tail = ((*queue)->tail + 1) % (*queue)->capacity;
    (*queue)->size++;

    return true;
}

void *sm3t__dequeue(sm3t_queue_t *queue) {
    if (sm3t__queue_empty(queue)) {
        return NULL;
    }

    void *data = queue->data[queue->head];
    queue->head = (queue->head + 1) % queue->size;
    return data;
}

bool sm3t__queue_full(sm3t_queue_t *queue) { return queue->capacity == queue->size && queue->capacity != 0; }

bool sm3t__queue_empty(sm3t_queue_t *queue) { return queue == NULL || queue->size == 0; }

void sm3t__free_queue(sm3t_queue_t **queue) {
    free(*queue);
    *queue = NULL;
}
