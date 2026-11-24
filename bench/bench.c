/* Performans ölçümleri: make bench   (hızlı sürüm: make bench-quick)
 *
 * 1) Veri yapıları: farklı n değerlerinde işlem başına süre (ns)
 * 2) Rota algoritmaları: sentetik ızgara şehirlerde (1K / 10K / 100K durak)
 *    Dijkstra, A* ve en az aktarma için ortalama sorgu süresi ve açılan durum
 * 3) Gerçek veri (M4/M5/M8): tüm durak çiftleri
 *
 * Sonuçlar ekrana tablo olarak, results/ds.csv ve results/routes.csv
 * dosyalarına rapor grafikleri için yazılır. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gen.h"
#include "graph.h"
#include "hash.h"
#include "heap.h"
#include "io_csv.h"
#include "queue.h"
#include "route.h"
#include "sort.h"
#include "stack.h"
#include "trie.h"

static volatile long g_sink;   /* derleyici ölçülen kodu silmesin */
static FILE *g_ds_csv, *g_route_csv;

static double now_sec(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

static unsigned g_rng = 12345;
static unsigned rnd(void)
{
    g_rng = g_rng * 1103515245u + 12345u;
    return g_rng >> 8;
}

static void report_ds(const char *structure, const char *op, int n, double seconds, long ops)
{
    double ns = seconds * 1e9 / (double)ops;
    printf("  %-14s %-22s n=%-8d %10.2f ms %10.1f ns/islem\n", structure, op, n, seconds * 1e3, ns);
    fprintf(g_ds_csv, "%s,%s,%d,%.4f,%.2f\n", structure, op, n, seconds * 1e3, ns);
}

/* ------------------------------------------------------------------ */
/* 1) Veri yapıları                                                   */
/* ------------------------------------------------------------------ */

static void bench_stack_queue(int n)
{
    Stack s;
    stack_init(&s, sizeof(int));
    double t = now_sec();
    for (int i = 0; i < n; i++) stack_push(&s, &i);
    for (int i = 0, v; i < n; i++) { stack_pop(&s, &v); g_sink += v; }
    report_ds("stack", "push+pop", n, now_sec() - t, 2L * n);
    stack_free(&s);

    Queue q;
    queue_init(&q, sizeof(int));
    t = now_sec();
    for (int i = 0; i < n; i++) queue_enqueue(&q, &i);
    for (int i = 0, v; i < n; i++) { queue_dequeue(&q, &v); g_sink += v; }
    report_ds("queue", "enqueue+dequeue", n, now_sec() - t, 2L * n);
    queue_free(&q);
}

static void bench_heap(int n)
{
    MinHeap h;
    heap_init(&h, n);
    double *prio = malloc(sizeof(double) * (size_t)n);
    for (int i = 0; i < n; i++) prio[i] = (double)(rnd() % 1000000u);

    double t = now_sec();
    for (int i = 0; i < n; i++) heap_push(&h, i, prio[i]);
    report_ds("min-heap", "push", n, now_sec() - t, n);

    t = now_sec();
    for (int i = 0; i < n; i += 2) heap_push(&h, i, prio[i] - 1e6);   /* decrease-key */
    report_ds("min-heap", "decrease-key", n, now_sec() - t, (n + 1) / 2);

    t = now_sec();
    int k;
    while (heap_pop(&h, &k, NULL)) g_sink += k;
    report_ds("min-heap", "pop", n, now_sec() - t, n);

    free(prio);
    heap_free(&h);
}

static char **make_keys(int n, int min_len, int max_len)
{
    char **keys = malloc(sizeof(char *) * (size_t)n);
    for (int i = 0; i < n; i++) {
        int len = min_len + (int)(rnd() % (unsigned)(max_len - min_len + 1));
        keys[i] = malloc((size_t)len + 1);
        for (int j = 0; j < len; j++) keys[i][j] = (char)('a' + rnd() % 26u);
        keys[i][len] = '\0';
    }
    return keys;
}

static void free_keys(char **keys, int n)
{
    for (int i = 0; i < n; i++) free(keys[i]);
    free(keys);
}

static void bench_hash(int n)
{
    char **keys = make_keys(n, 6, 12);
    HashMap m;
    hash_init(&m, 16);

    double t = now_sec();
    for (int i = 0; i < n; i++) hash_put(&m, keys[i], i);
    report_ds("hash", "put (rehash dahil)", n, now_sec() - t, n);

    t = now_sec();
    for (int i = 0, v; i < n; i++) if (hash_get(&m, keys[i], &v)) g_sink += v;
    report_ds("hash", "get (bulunan)", n, now_sec() - t, n);

    hash_free(&m);
    free_keys(keys, n);
}

static void bench_trie(int n)
{
    char **keys = make_keys(n, 6, 12);
    Trie t;
    trie_init(&t);

    double t0 = now_sec();
    for (int i = 0; i < n; i++) trie_insert(&t, keys[i], i);
    report_ds("trie", "insert", n, now_sec() - t0, n);

    enum { Q = 1000 };
    int out[10];
    t0 = now_sec();
    for (int i = 0; i < Q; i++) {
        char prefix[4] = {(char)('a' + rnd() % 26u), (char)('a' + rnd() % 26u), (char)('a' + rnd() % 26u), 0};
        g_sink += trie_prefix(&t, prefix, out, 10);
    }
    report_ds("trie", "prefix (3 harf, ilk 10)", n, now_sec() - t0, Q);

    double mb = (double)t.node_count * sizeof(TrieNode) / (1024.0 * 1024.0);
    printf("  %-14s %zu dugum, ~%.1f MB (anahtar basina %.1f dugum)\n", "", t.node_count, mb,
           (double)t.node_count / n);
    fprintf(g_ds_csv, "trie,bellek_MB,%d,%.2f,\n", n, mb);

    trie_free(&t);
    free_keys(keys, n);
}

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static void bench_sort_search(int n)
{
    int *orig = malloc(sizeof(int) * (size_t)n), *a = malloc(sizeof(int) * (size_t)n);
    for (int i = 0; i < n; i++) orig[i] = (int)rnd();

    struct { const char *name; void (*fn)(void *, size_t, size_t, CompareFn); } sorts[] = {
        {"quick_sort", quick_sort}, {"merge_sort", merge_sort}, {"qsort (C kut.)", qsort},
    };
    for (size_t s = 0; s < sizeof sorts / sizeof sorts[0]; s++) {
        memcpy(a, orig, sizeof(int) * (size_t)n);
        double t = now_sec();
        sorts[s].fn(a, (size_t)n, sizeof(int), cmp_int);
        report_ds(sorts[s].name, "rastgele dizi", n, now_sec() - t, n);
    }
    /* Sıralı girdi: üçün ortancası pivotun etkisi */
    double t = now_sec();
    quick_sort(a, (size_t)n, sizeof(int), cmp_int);
    report_ds("quick_sort", "zaten sirali dizi", n, now_sec() - t, n);

    t = now_sec();
    for (int i = 0; i < n; i++) g_sink += binary_search(a, (size_t)n, sizeof(int), &orig[i], cmp_int);
    report_ds("binary_search", "arama (bulunan)", n, now_sec() - t, n);

    free(orig);
    free(a);
}

/* ------------------------------------------------------------------ */
/* 2-3) Rota algoritmaları                                            */
/* ------------------------------------------------------------------ */

typedef bool (*RouteFn)(const Network *, int, int, Route *);

typedef struct { int a, b; } Pair;

/* Aynı sorgu kümesini her algoritmayla çalıştırır, A*'ın sonucunu
 * Dijkstra ile karşılaştırır. */
static void bench_routes(const char *dataset, const Network *net, const Pair *q, int nq)
{
    struct { const char *name; RouteFn fn; } algos[] = {
        {"Dijkstra", route_fastest},
        {"A*", route_astar},
        {"En az aktarma", route_min_transfers},
    };
    double *dijkstra_min = malloc(sizeof(double) * (size_t)nq);
    int mismatches = 0;

    printf("\n  %s: %d durak, %d durum, %d hat, %d sorgu\n", dataset, net->stop_count, net->state_count,
           net->line_count, nq);
    for (size_t k = 0; k < sizeof algos / sizeof algos[0]; k++) {
        long expanded = 0;
        double t = now_sec();
        for (int i = 0; i < nq; i++) {
            Route r;
            algos[k].fn(net, q[i].a, q[i].b, &r);
            expanded += r.expanded;
            if (k == 0) dijkstra_min[i] = r.total_min;
            if (k == 1 && (r.total_min - dijkstra_min[i] > 1e-6 || dijkstra_min[i] - r.total_min > 1e-6))
                mismatches++;
            route_free(&r);
        }
        double ms = (now_sec() - t) * 1e3 / nq;
        double avg_exp = (double)expanded / nq;
        printf("    %-14s ort. %9.3f ms/sorgu   ort. %10.1f acilan durum (%%%5.1f)\n", algos[k].name, ms,
               avg_exp, 100.0 * avg_exp / net->state_count);
        fprintf(g_route_csv, "%s,%d,%d,%s,%d,%.4f,%.1f\n", dataset, net->stop_count, net->state_count,
                algos[k].name, nq, ms, avg_exp);
    }
    printf("    A* sonucu Dijkstra ile %s\n", mismatches ? "UYUSMUYOR!" : "tum sorgularda ayni");
    free(dijkstra_min);
}

static void bench_grid(int size, int nq)
{
    Network net;
    network_init(&net);
    double t = now_sec();
    gen_grid_city(&net, size, 42);
    double build = now_sec() - t;

    Pair *q = malloc(sizeof(Pair) * (size_t)nq);
    for (int i = 0; i < nq; i++) {
        q[i].a = (int)(rnd() % (unsigned)net.stop_count);
        q[i].b = (int)(rnd() % (unsigned)net.stop_count);
    }
    char name[32];
    snprintf(name, sizeof name, "Izgara %dx%d", size, size);
    bench_routes(name, &net, q, nq);
    printf("    (ag kurulumu %.1f ms)\n", build * 1e3);

    free(q);
    network_free(&net);
}

static void bench_real_data(void)
{
    Network net;
    network_init(&net);
    if (!network_load(&net, "data", stderr, NULL)) {
        fprintf(stderr, "data/ yuklenemedi, gercek veri olcumu atlandi\n");
        network_free(&net);
        return;
    }
    int nq = net.stop_count * net.stop_count;
    Pair *q = malloc(sizeof(Pair) * (size_t)nq);
    for (int a = 0, i = 0; a < net.stop_count; a++)
        for (int b = 0; b < net.stop_count; b++, i++) q[i] = (Pair){a, b};
    bench_routes("M4+M5+M8 (tum ciftler)", &net, q, nq);
    free(q);
    network_free(&net);
}

int main(int argc, char **argv)
{
    bool quick = argc > 1 && strcmp(argv[1], "--quick") == 0;

    g_ds_csv = fopen("results/ds.csv", "w");
    g_route_csv = fopen("results/routes.csv", "w");
    if (!g_ds_csv || !g_route_csv) {
        fprintf(stderr, "results/ klasorune yazilamadi (make bench ile calistirin)\n");
        return EXIT_FAILURE;
    }
    fprintf(g_ds_csv, "yapi,islem,n,toplam_ms,ns_per_islem\n");
    fprintf(g_route_csv, "veri,durak,durum,algoritma,sorgu,ort_ms,ort_acilan_durum\n");

    printf("=== 1) Veri yapilari ===\n");
    int sizes_full[] = {10000, 100000, 1000000}, sizes_quick[] = {1000, 10000};
    int *sizes = quick ? sizes_quick : sizes_full;
    int nsizes = quick ? 2 : 3;
    for (int i = 0; i < nsizes; i++) {
        int n = sizes[i];
        printf("\n");
        bench_stack_queue(n);
        bench_heap(n);
        bench_hash(n);
        if (n <= 100000) bench_trie(n);   /* 10^6 rastgele anahtar ~2 GB trie: atlandı */
        bench_sort_search(n);
    }

    printf("\n=== 2) Rota algoritmalari: sentetik izgara sehirler ===\n");
    bench_grid(32, quick ? 50 : 300);          /* ~1K durak   */
    bench_grid(100, quick ? 10 : 100);         /* 10K durak   */
    if (!quick) bench_grid(316, 20);           /* ~100K durak */

    printf("\n=== 3) Gercek veri ===\n");
    bench_real_data();

    fclose(g_ds_csv);
    fclose(g_route_csv);
    printf("\nSonuclar: results/ds.csv, results/routes.csv\n");
    return EXIT_SUCCESS;
}
