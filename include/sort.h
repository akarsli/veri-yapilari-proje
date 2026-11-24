#ifndef SORT_H
#define SORT_H

#include <stddef.h>

/* Genel amaçlı sıralama ve arama (qsort/bsearch ile aynı arayüz).
 * cmp(a, b): a < b ise negatif, eşitse 0, a > b ise pozitif.
 *
 * merge_sort    : O(n log n) her durumda, O(n) ek bellek, KARARLI (stable).
 *                 Rotaları süreye göre sıralarken eşit süreli rotaların
 *                 önceki sırası (ör. aktarma sayısı) korunur.
 * quick_sort    : ortalama O(n log n), en kötü O(n^2); ek bellek O(log n).
 *                 Üçün ortancası pivot + küçük parçalarda insertion sort.
 * binary_search : O(log n); dizi aynı cmp ile sıralı olmalı.
 *                 Bulursa indeks, bulamazsa -1 döner. */
typedef int (*CompareFn)(const void *a, const void *b);

void merge_sort(void *base, size_t n, size_t size, CompareFn cmp);
void quick_sort(void *base, size_t n, size_t size, CompareFn cmp);
long binary_search(const void *base, size_t n, size_t size, const void *key, CompareFn cmp);

#endif
