#include "sort.h"

#include <stdlib.h>
#include <string.h>

#include "common.h"

#define AT(arr, i) ((arr) + (size_t)(i) * size)

/* ------------------------------------------------------------------ */
/* Mergesort                                                          */
/* ------------------------------------------------------------------ */

/* [lo, mid) ve [mid, hi) sıralı parçalarını tmp yardımıyla birleştirir. */
static void merge(unsigned char *arr, unsigned char *tmp, size_t lo, size_t mid,
                  size_t hi, size_t size, CompareFn cmp)
{
    size_t i = lo, j = mid, k = 0;
    while (i < mid && j < hi) {
        /* <= : eşitlikte soldakini al -> kararlılık */
        if (cmp(AT(arr, i), AT(arr, j)) <= 0) memcpy(AT(tmp, k++), AT(arr, i++), size);
        else                                  memcpy(AT(tmp, k++), AT(arr, j++), size);
    }
    if (i < mid) {
        memcpy(AT(tmp, k), AT(arr, i), (mid - i) * size);
        k += mid - i;
    }
    if (j < hi) {
        memcpy(AT(tmp, k), AT(arr, j), (hi - j) * size);
        k += hi - j;
    }
    memcpy(AT(arr, lo), tmp, k * size);
}

static void merge_sort_rec(unsigned char *arr, unsigned char *tmp, size_t lo, size_t hi,
                           size_t size, CompareFn cmp)
{
    if (hi - lo < 2) return;
    size_t mid = lo + (hi - lo) / 2;
    merge_sort_rec(arr, tmp, lo, mid, size, cmp);
    merge_sort_rec(arr, tmp, mid, hi, size, cmp);
    if (cmp(AT(arr, mid - 1), AT(arr, mid)) <= 0) return;   /* zaten sıralı */
    merge(arr, tmp, lo, mid, hi, size, cmp);
}

void merge_sort(void *base, size_t n, size_t size, CompareFn cmp)
{
    if (n < 2) return;
    unsigned char *tmp = xmalloc(n * size);   /* tek seferlik O(n) tampon */
    merge_sort_rec(base, tmp, 0, n, size, cmp);
    free(tmp);
}

/* ------------------------------------------------------------------ */
/* Quicksort                                                          */
/* ------------------------------------------------------------------ */

#define INSERTION_THRESHOLD 16

static void swap_bytes(unsigned char *a, unsigned char *b, size_t size)
{
    if (a == b) return;
    while (size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

static void insertion_sort(unsigned char *arr, size_t n, size_t size, CompareFn cmp,
                           unsigned char *tmp)
{
    for (size_t i = 1; i < n; i++) {
        memcpy(tmp, AT(arr, i), size);
        size_t j = i;
        while (j > 0 && cmp(AT(arr, j - 1), tmp) > 0) {
            memcpy(AT(arr, j), AT(arr, j - 1), size);
            j--;
        }
        memcpy(AT(arr, j), tmp, size);
    }
}

static void quick_sort_rec(unsigned char *arr, size_t n, size_t size, CompareFn cmp,
                           unsigned char *pivot)
{
    while (n > INSERTION_THRESHOLD) {
        /* Üçün ortancası: arr[0] <= arr[mid] <= arr[n-1] olacak şekilde düzenle.
         * Sıralı/ters sıralı girdide O(n^2)'ye düşmeyi engeller. */
        size_t mid = (n - 1) / 2;
        if (cmp(AT(arr, mid), AT(arr, 0)) < 0)     swap_bytes(AT(arr, mid), AT(arr, 0), size);
        if (cmp(AT(arr, n - 1), AT(arr, 0)) < 0)   swap_bytes(AT(arr, n - 1), AT(arr, 0), size);
        if (cmp(AT(arr, n - 1), AT(arr, mid)) < 0) swap_bytes(AT(arr, n - 1), AT(arr, mid), size);
        memcpy(pivot, AT(arr, mid), size);

        /* Hoare bölümlemesi: sonunda [0..j] <= pivot <= [j+1..n-1] */
        size_t i = 0, j = n - 1;
        for (;;) {
            while (cmp(AT(arr, i), pivot) < 0) i++;
            while (cmp(AT(arr, j), pivot) > 0) j--;
            if (i >= j) break;
            swap_bytes(AT(arr, i), AT(arr, j), size);
            i++;
            j--;
        }

        /* Küçük parçada özyineleme, büyük parçada döngü: yığın derinliği O(log n) */
        size_t left_n = j + 1, right_n = n - left_n;
        if (left_n < right_n) {
            quick_sort_rec(arr, left_n, size, cmp, pivot);
            arr = AT(arr, left_n);
            n = right_n;
        } else {
            quick_sort_rec(AT(arr, left_n), right_n, size, cmp, pivot);
            n = left_n;
        }
    }
    insertion_sort(arr, n, size, cmp, pivot);
}

void quick_sort(void *base, size_t n, size_t size, CompareFn cmp)
{
    if (n < 2) return;
    unsigned char *pivot = xmalloc(size);
    quick_sort_rec(base, n, size, cmp, pivot);
    free(pivot);
}

/* ------------------------------------------------------------------ */
/* Binary search                                                      */
/* ------------------------------------------------------------------ */

long binary_search(const void *base, size_t n, size_t size, const void *key, CompareFn cmp)
{
    const unsigned char *arr = base;
    size_t lo = 0, hi = n;            /* aranan aralık: [lo, hi) */
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;   /* taşmayı önler */
        int c = cmp(AT(arr, mid), key);
        if (c == 0) return (long)mid;
        if (c < 0) lo = mid + 1;
        else       hi = mid;
    }
    return -1;
}
