#include "stack.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"

#define STACK_INITIAL_CAP 16

void stack_init(Stack *s, size_t elem_size)
{
    s->data = NULL;
    s->elem_size = elem_size;
    s->size = 0;
    s->cap = 0;
}

void stack_free(Stack *s)
{
    free(s->data);
    s->data = NULL;
    s->size = s->cap = 0;
}

void stack_clear(Stack *s)
{
    s->size = 0;
}

void stack_push(Stack *s, const void *elem)
{
    if (s->size == s->cap) {
        /* Kapasiteyi ikiye katla: amortize O(1) push */
        s->cap = s->cap ? s->cap * 2 : STACK_INITIAL_CAP;
        s->data = xrealloc(s->data, s->cap * s->elem_size);
    }
    memcpy(s->data + s->size * s->elem_size, elem, s->elem_size);
    s->size++;
}

bool stack_pop(Stack *s, void *out)
{
    if (s->size == 0) return false;
    s->size--;
    if (out) memcpy(out, s->data + s->size * s->elem_size, s->elem_size);
    return true;
}

bool stack_peek(const Stack *s, void *out)
{
    if (s->size == 0) return false;
    memcpy(out, s->data + (s->size - 1) * s->elem_size, s->elem_size);
    return true;
}

bool stack_empty(const Stack *s)
{
    return s->size == 0;
}

size_t stack_size(const Stack *s)
{
    return s->size;
}
