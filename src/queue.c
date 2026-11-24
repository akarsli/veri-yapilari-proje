#include "queue.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"

#define QUEUE_INITIAL_CAP 16

static unsigned char *slot(const Queue *q, size_t logical_index)
{
    return q->data + ((q->head + logical_index) % q->cap) * q->elem_size;
}

/* Kapasiteyi ikiye katlar ve elemanları yeni dizinin başına sırayla taşır. */
static void grow(Queue *q)
{
    size_t new_cap = q->cap ? q->cap * 2 : QUEUE_INITIAL_CAP;
    unsigned char *new_data = xmalloc(new_cap * q->elem_size);
    for (size_t i = 0; i < q->size; i++)
        memcpy(new_data + i * q->elem_size, slot(q, i), q->elem_size);
    free(q->data);
    q->data = new_data;
    q->cap = new_cap;
    q->head = 0;
}

void queue_init(Queue *q, size_t elem_size)
{
    q->data = NULL;
    q->elem_size = elem_size;
    q->head = q->size = q->cap = 0;
}

void queue_free(Queue *q)
{
    free(q->data);
    q->data = NULL;
    q->head = q->size = q->cap = 0;
}

void queue_clear(Queue *q)
{
    q->head = q->size = 0;
}

void queue_enqueue(Queue *q, const void *elem)
{
    if (q->size == q->cap) grow(q);
    memcpy(slot(q, q->size), elem, q->elem_size);
    q->size++;
}

void queue_push_front(Queue *q, const void *elem)
{
    if (q->size == q->cap) grow(q);
    q->head = (q->head + q->cap - 1) % q->cap;
    memcpy(q->data + q->head * q->elem_size, elem, q->elem_size);
    q->size++;
}

bool queue_dequeue(Queue *q, void *out)
{
    if (q->size == 0) return false;
    if (out) memcpy(out, slot(q, 0), q->elem_size);
    q->head = (q->head + 1) % q->cap;
    q->size--;
    return true;
}

bool queue_peek(const Queue *q, void *out)
{
    if (q->size == 0) return false;
    memcpy(out, slot(q, 0), q->elem_size);
    return true;
}

bool queue_empty(const Queue *q)
{
    return q->size == 0;
}

size_t queue_size(const Queue *q)
{
    return q->size;
}
