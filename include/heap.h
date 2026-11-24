#ifndef HEAP_H
#define HEAP_H

#include <stdbool.h>

/* İndeksli ikili min-heap (öncelik kuyruğu).
 *
 * Anahtarlar 0 .. capacity-1 aralığındaki tamsayılardır (Dijkstra'da her
 * (durak, hat) durumu bir anahtar). pos[] dizisi her anahtarın heap içindeki
 * yerini tuttuğu için "decrease-key" O(log n) yapılabilir; aynı düğüm heap'e
 * tekrar tekrar eklenmez.
 *
 * Karmaşıklık: push / decrease-key / pop O(log n), contains/empty O(1),
 * bellek O(capacity). */
typedef struct {
    int    *heap;  /* heap[i]  = i. konumdaki anahtar           */
    int    *pos;   /* pos[key] = anahtarın konumu, yoksa -1     */
    double *prio;  /* prio[key] = anahtarın önceliği (maliyet)  */
    int     size;
    int     capacity;
} MinHeap;

void heap_init(MinHeap *h, int capacity);
void heap_free(MinHeap *h);
void heap_clear(MinHeap *h);
bool heap_empty(const MinHeap *h);
int  heap_size(const MinHeap *h);
bool heap_contains(const MinHeap *h, int key);

/* Anahtar yoksa ekler; varsa ve yeni öncelik daha küçükse azaltır.
 * Bir değişiklik yapıldıysa true döner. */
bool heap_push(MinHeap *h, int key, double priority);

/* En küçük öncelikli anahtarı çıkarır. Heap boşsa false döner. */
bool heap_pop(MinHeap *h, int *key, double *priority);

#endif
