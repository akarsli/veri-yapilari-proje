#ifndef QUEUE_H
#define QUEUE_H

#include <stdbool.h>
#include <stddef.h>

/* Dairesel dizi (ring buffer) tabanlı genel amaçlı kuyruk.
 *
 * Normal kuyruk işlemleri (enqueue/dequeue) BFS ve toplu rota sorgularında
 * kullanılır. Ek olarak öne ekleme (push_front) desteklenir; böylece aynı yapı
 * "en az aktarma" aramasındaki 0-1 BFS için çift uçlu kuyruk (deque) olur:
 * maliyeti 0 olan kenar öne, maliyeti 1 olan kenar arkaya eklenir.
 *
 * Karmaşıklık: enqueue/push_front O(1) amortize, dequeue/peek O(1). */
typedef struct {
    unsigned char *data;
    size_t elem_size;
    size_t head;   /* ilk elemanın indeksi */
    size_t size;
    size_t cap;
} Queue;

void   queue_init(Queue *q, size_t elem_size);
void   queue_free(Queue *q);
void   queue_clear(Queue *q);
void   queue_enqueue(Queue *q, const void *elem);    /* arkaya ekle */
void   queue_push_front(Queue *q, const void *elem); /* öne ekle (deque) */
bool   queue_dequeue(Queue *q, void *out);           /* önden çıkar; out NULL olabilir */
bool   queue_peek(const Queue *q, void *out);
bool   queue_empty(const Queue *q);
size_t queue_size(const Queue *q);

#endif
