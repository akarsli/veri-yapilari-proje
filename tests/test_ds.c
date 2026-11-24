/* Temel veri yapıları için birim testleri: make test */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"
#include "heap.h"
#include "queue.h"
#include "sort.h"
#include "stack.h"
#include "text.h"
#include "trie.h"

static int g_checks = 0, g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            fprintf(stderr, "  BASARISIZ %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

/* ---------------------------------------------------------------- */

static void test_stack(void)
{
    Stack s;
    stack_init(&s, sizeof(int));
    CHECK(stack_empty(&s));
    CHECK(!stack_pop(&s, NULL));

    for (int i = 0; i < 1000; i++) stack_push(&s, &i);
    CHECK(stack_size(&s) == 1000);

    int top;
    CHECK(stack_peek(&s, &top) && top == 999);
    int ok = 1;
    for (int i = 999; i >= 0; i--) {
        int v;
        if (!stack_pop(&s, &v) || v != i) ok = 0;   /* LIFO */
    }
    CHECK(ok);
    CHECK(stack_empty(&s));

    /* Yapı (struct) saklama: undo/redo kayıtları için */
    typedef struct { int from, to; char line[4]; } Op;
    Stack ops;
    stack_init(&ops, sizeof(Op));
    Op a = {1, 2, "M4"}, b = {7, 8, "M8"}, out;
    stack_push(&ops, &a);
    stack_push(&ops, &b);
    CHECK(stack_pop(&ops, &out) && out.from == 7 && strcmp(out.line, "M8") == 0);
    CHECK(stack_pop(&ops, &out) && out.to == 2 && strcmp(out.line, "M4") == 0);

    stack_free(&ops);
    stack_free(&s);
}

static void test_queue(void)
{
    Queue q;
    queue_init(&q, sizeof(int));
    CHECK(queue_empty(&q));
    CHECK(!queue_dequeue(&q, NULL));

    /* Dairesel tamponun sarmasını ve büyümesini zorlayan karışık işlemler */
    int next_in = 0, next_out = 0, ok = 1;
    for (int round = 0; round < 50; round++) {
        for (int i = 0; i < 37; i++) queue_enqueue(&q, &(int){next_in++});
        for (int i = 0; i < 30; i++) {
            int v;
            if (!queue_dequeue(&q, &v) || v != next_out++) ok = 0;   /* FIFO */
        }
    }
    CHECK(ok);
    CHECK(queue_size(&q) == (size_t)(next_in - next_out));
    while (queue_dequeue(&q, NULL)) {}
    CHECK(queue_empty(&q));

    /* Deque davranışı (0-1 BFS): öne eklenen önce çıkar */
    int v;
    queue_enqueue(&q, &(int){1});
    queue_enqueue(&q, &(int){2});
    queue_push_front(&q, &(int){0});
    CHECK(queue_peek(&q, &v) && v == 0);
    CHECK(queue_dequeue(&q, &v) && v == 0);
    CHECK(queue_dequeue(&q, &v) && v == 1);
    CHECK(queue_dequeue(&q, &v) && v == 2);

    /* Boş kuyruğa öne ekleme */
    queue_push_front(&q, &(int){42});
    CHECK(queue_dequeue(&q, &v) && v == 42);

    queue_free(&q);
}

static void test_heap(void)
{
    MinHeap h;
    heap_init(&h, 100);
    CHECK(heap_empty(&h));

    heap_push(&h, 5, 50.0);
    heap_push(&h, 3, 30.0);
    heap_push(&h, 8, 80.0);
    heap_push(&h, 1, 10.0);
    CHECK(heap_size(&h) == 4);
    CHECK(heap_contains(&h, 8) && !heap_contains(&h, 9));

    /* decrease-key: 8'in önceliği 80 -> 5, artık en küçük */
    CHECK(heap_push(&h, 8, 5.0));
    /* Daha büyük öncelik verilirse değişmemeli */
    CHECK(!heap_push(&h, 3, 99.0));
    CHECK(heap_size(&h) == 4);

    int k;
    double p;
    CHECK(heap_pop(&h, &k, &p) && k == 8 && p == 5.0);
    CHECK(heap_pop(&h, &k, &p) && k == 1);
    CHECK(heap_pop(&h, &k, &p) && k == 3 && p == 30.0);
    CHECK(heap_pop(&h, &k, &p) && k == 5);
    CHECK(!heap_pop(&h, &k, &p));
    CHECK(!heap_contains(&h, 5));

    /* Rastgele öncelikler: çıkış sırası artan olmalı */
    srand(12345);
    for (int key = 0; key < 100; key++) heap_push(&h, key, (double)(rand() % 1000));
    for (int key = 0; key < 100; key += 3) heap_push(&h, key, -1.0 * key);   /* decrease */
    double prev = -1e18;
    int ok = 1, count = 0;
    while (heap_pop(&h, &k, &p)) {
        if (p < prev) ok = 0;
        prev = p;
        count++;
    }
    CHECK(ok && count == 100);

    heap_push(&h, 7, 1.0);
    heap_clear(&h);
    CHECK(heap_empty(&h) && !heap_contains(&h, 7));
    CHECK(!heap_push(&h, 100, 1.0));   /* kapasite dışı anahtar */

    heap_free(&h);
}

static void test_text(void)
{
    char buf[128];
    text_normalize("Kadıköy", buf, sizeof buf);
    CHECK(strcmp(buf, "kadikoy") == 0);
    text_normalize("  ÜSKÜDAR ", buf, sizeof buf);
    CHECK(strcmp(buf, "uskudar") == 0);
    text_normalize("Yakacık-Adnan Kahveci", buf, sizeof buf);
    CHECK(strcmp(buf, "yakacik adnan kahveci") == 0);
    text_normalize("İçerenköy", buf, sizeof buf);
    CHECK(strcmp(buf, "icerenkoy") == 0);
    text_normalize("Ihlamurkuyu", buf, sizeof buf);
    CHECK(strcmp(buf, "ihlamurkuyu") == 0);
    text_normalize("Ayşekadın  --  Şeyhli", buf, sizeof buf);
    CHECK(strcmp(buf, "aysekadin seyhli") == 0);
    text_normalize("MODOKO-KEYAP", buf, sizeof buf);
    CHECK(strcmp(buf, "modoko keyap") == 0);

    char small[5];
    text_normalize("Bostancı", small, sizeof small);   /* taşma olmamalı */
    CHECK(strcmp(small, "bost") == 0);
}

static void test_hash(void)
{
    HashMap m;
    hash_init(&m, 4);   /* küçük başlat: rehash'i zorla */
    const char *names[] = {"kadikoy", "uskudar", "kozyatagi", "dudullu", "bostanci", "parseller"};
    for (int i = 0; i < 6; i++) hash_put(&m, names[i], i);
    CHECK(hash_size(&m) == 6);

    int v;
    CHECK(hash_get(&m, "kozyatagi", &v) && v == 2);
    CHECK(!hash_get(&m, "taksim", &v));

    hash_put(&m, "kozyatagi", 99);   /* güncelleme, yeni kayıt değil */
    CHECK(hash_size(&m) == 6 && hash_get(&m, "kozyatagi", &v) && v == 99);

    CHECK(hash_remove(&m, "dudullu"));
    CHECK(!hash_remove(&m, "dudullu"));
    CHECK(!hash_get(&m, "dudullu", NULL));
    CHECK(hash_size(&m) == 5);

    /* Çok sayıda anahtar */
    char key[32];
    for (int i = 0; i < 5000; i++) {
        snprintf(key, sizeof key, "durak %d", i);
        hash_put(&m, key, i);
    }
    int ok = 1;
    for (int i = 0; i < 5000; i++) {
        snprintf(key, sizeof key, "durak %d", i);
        if (!hash_get(&m, key, &v) || v != i) ok = 0;
    }
    CHECK(ok);
    CHECK(hash_size(&m) == 5005);

    hash_free(&m);
}

static void test_trie(void)
{
    Trie t;
    trie_init(&t);
    const char *raw[] = {"Kadıköy", "Kartal", "Kayışdağı", "Kozyatağı", "Küçükyalı",
                         "Küçükbakkalköy", "Kurtköy", "Kısıklı", "Üsküdar"};
    char key[64];
    for (int i = 0; i < 9; i++) {
        text_normalize(raw[i], key, sizeof key);
        CHECK(trie_insert(&t, key, i));
    }
    CHECK(!trie_insert(&t, "Buyuk Harf", 99));   /* normalleştirilmemiş anahtar reddedilir */

    int out[16];
    int n = trie_prefix(&t, "ka", out, 16);
    /* alfabetik: kadikoy, kartal, kayisdagi */
    CHECK(n == 3 && out[0] == 0 && out[1] == 1 && out[2] == 2);

    n = trie_prefix(&t, "kucuk", out, 16);
    /* kucukbakkalkoy (5) < kucukyali (4) */
    CHECK(n == 2 && out[0] == 5 && out[1] == 4);

    n = trie_prefix(&t, "k", out, 2);   /* max sınırı */
    CHECK(n == 2);

    CHECK(trie_prefix(&t, "taksim", out, 16) == 0);
    CHECK(trie_prefix(&t, "", out, 16) == 9);   /* boş önek: hepsi */

    CHECK(trie_find_exact(&t, "kartal", out, 16) == 1 && out[0] == 1);
    CHECK(trie_find_exact(&t, "kart", out, 16) == 0);

    /* Aynı ada sahip iki kayıt */
    trie_insert(&t, "kartal", 42);
    CHECK(trie_find_exact(&t, "kartal", out, 16) == 2 && out[1] == 42);

    trie_free(&t);
}

/* ---------------------------------------------------------------- */

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

typedef struct { int minutes; int order; } Route;

static int cmp_route_minutes(const void *a, const void *b)
{
    const Route *x = a, *y = b;
    return (x->minutes > y->minutes) - (x->minutes < y->minutes);
}

static int is_sorted(const int *a, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (a[i - 1] > a[i]) return 0;
    return 1;
}

static void check_sort(void (*sort)(void *, size_t, size_t, CompareFn), const char *name)
{
    enum { N = 20000 };
    int *a = malloc(sizeof(int) * N), *ref = malloc(sizeof(int) * N);
    const char *cases[] = {"rastgele", "sirali", "ters", "cok tekrar", "hepsi esit"};
    for (int c = 0; c < 5; c++) {
        for (size_t n = 0; n <= N; n = n ? n * 3 + 1 : 1) {   /* 0,1,4,13,...,N'e kadar */
            for (size_t i = 0; i < n; i++) {
                switch (c) {
                case 0: a[i] = rand(); break;
                case 1: a[i] = (int)i; break;
                case 2: a[i] = (int)(n - i); break;
                case 3: a[i] = rand() % 5; break;
                default: a[i] = 7; break;
                }
            }
            memcpy(ref, a, sizeof(int) * n);
            sort(a, n, sizeof(int), cmp_int);
            qsort(ref, n, sizeof(int), cmp_int);
            if (!is_sorted(a, n) || memcmp(a, ref, sizeof(int) * n) != 0) {
                fprintf(stderr, "  %s hatasi: durum=%s n=%zu\n", name, cases[c], n);
                g_failures++;
            }
            g_checks++;
        }
    }
    free(a);
    free(ref);
}

static void test_sort(void)
{
    srand(2026);
    check_sort(merge_sort, "merge_sort");
    check_sort(quick_sort, "quick_sort");

    /* Mergesort kararlılığı: eşit süreli rotalar önceki sırasını korumalı */
    Route r[] = {{30, 0}, {25, 1}, {30, 2}, {25, 3}, {40, 4}, {30, 5}};
    merge_sort(r, 6, sizeof(Route), cmp_route_minutes);
    CHECK(r[0].order == 1 && r[1].order == 3);
    CHECK(r[2].order == 0 && r[3].order == 2 && r[4].order == 5);
    CHECK(r[5].order == 4);

    /* Binary search */
    int s[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29};
    for (int i = 0; i < 10; i++) CHECK(binary_search(s, 10, sizeof(int), &s[i], cmp_int) == i);
    int missing[] = {0, 4, 30, 18};
    for (int i = 0; i < 4; i++) CHECK(binary_search(s, 10, sizeof(int), &missing[i], cmp_int) == -1);
    CHECK(binary_search(s, 0, sizeof(int), &s[0], cmp_int) == -1);
}

int main(void)
{
    struct { const char *name; void (*fn)(void); } tests[] = {
        {"stack", test_stack}, {"queue", test_queue}, {"heap", test_heap},
        {"text", test_text},   {"hash", test_hash},   {"trie", test_trie},
        {"sort", test_sort},
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = g_failures;
        tests[i].fn();
        printf("%-6s %s\n", tests[i].name, g_failures == before ? "OK" : "HATA");
    }
    printf("\n%d kontrol, %d hata\n", g_checks, g_failures);
    return g_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
