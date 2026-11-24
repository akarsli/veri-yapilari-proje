#include "heap.h"

#include <stdlib.h>

#include "common.h"

#define PARENT(i) (((i) - 1) / 2)
#define LEFT(i)   (2 * (i) + 1)

static void swap_nodes(MinHeap *h, int a, int b)
{
    int ka = h->heap[a], kb = h->heap[b];
    h->heap[a] = kb;
    h->heap[b] = ka;
    h->pos[kb] = a;
    h->pos[ka] = b;
}

static void sift_up(MinHeap *h, int i)
{
    while (i > 0 && h->prio[h->heap[i]] < h->prio[h->heap[PARENT(i)]]) {
        swap_nodes(h, i, PARENT(i));
        i = PARENT(i);
    }
}

static void sift_down(MinHeap *h, int i)
{
    for (;;) {
        int smallest = i;
        int l = LEFT(i), r = l + 1;
        if (l < h->size && h->prio[h->heap[l]] < h->prio[h->heap[smallest]]) smallest = l;
        if (r < h->size && h->prio[h->heap[r]] < h->prio[h->heap[smallest]]) smallest = r;
        if (smallest == i) return;
        swap_nodes(h, i, smallest);
        i = smallest;
    }
}

void heap_init(MinHeap *h, int capacity)
{
    h->heap = xmalloc(sizeof(int) * (size_t)capacity);
    h->pos = xmalloc(sizeof(int) * (size_t)capacity);
    h->prio = xmalloc(sizeof(double) * (size_t)capacity);
    h->size = 0;
    h->capacity = capacity;
    for (int k = 0; k < capacity; k++) h->pos[k] = -1;
}

void heap_free(MinHeap *h)
{
    free(h->heap);
    free(h->pos);
    free(h->prio);
    h->heap = h->pos = NULL;
    h->prio = NULL;
    h->size = h->capacity = 0;
}

void heap_clear(MinHeap *h)
{
    /* Sadece heap'te kalan anahtarların konumunu sıfırla: O(size) */
    for (int i = 0; i < h->size; i++) h->pos[h->heap[i]] = -1;
    h->size = 0;
}

bool heap_empty(const MinHeap *h)
{
    return h->size == 0;
}

int heap_size(const MinHeap *h)
{
    return h->size;
}

bool heap_contains(const MinHeap *h, int key)
{
    return key >= 0 && key < h->capacity && h->pos[key] != -1;
}

bool heap_push(MinHeap *h, int key, double priority)
{
    if (key < 0 || key >= h->capacity) return false;

    if (h->pos[key] == -1) {             /* yeni anahtar: sona ekle, yukarı taşı */
        h->heap[h->size] = key;
        h->pos[key] = h->size;
        h->prio[key] = priority;
        h->size++;
        sift_up(h, h->pos[key]);
        return true;
    }
    if (priority < h->prio[key]) {       /* decrease-key */
        h->prio[key] = priority;
        sift_up(h, h->pos[key]);
        return true;
    }
    return false;
}

bool heap_pop(MinHeap *h, int *key, double *priority)
{
    if (h->size == 0) return false;

    int top = h->heap[0];
    if (key) *key = top;
    if (priority) *priority = h->prio[top];

    h->size--;
    if (h->size > 0) {
        h->heap[0] = h->heap[h->size];   /* son elemanı köke taşı, aşağı indir */
        h->pos[h->heap[0]] = 0;
        sift_down(h, 0);
    }
    h->pos[top] = -1;
    return true;
}
