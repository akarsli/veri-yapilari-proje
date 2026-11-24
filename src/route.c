#include "route.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "heap.h"
#include "queue.h"
#include "stack.h"

static double half_headway(const Network *net, int state)
{
    return net->lines[net->states[state].line].headway_min / 2.0;
}

const RouteWeights WEIGHTS_FASTEST     = {.time = 1.0, .transfer = 0.0, .crowd = 0.0};
const RouteWeights WEIGHTS_BALANCED    = {.time = 1.0, .transfer = 5.0, .crowd = 0.5};
const RouteWeights WEIGHTS_AVOID_CROWD = {.time = 1.0, .transfer = 2.0, .crowd = 3.0};

/* Bir kenarın süresi (dakika): yolculuk, ya da yürüme + yeni hattı bekleme. */
static double edge_cost(const Network *net, const Edge *e)
{
    if (e->kind == EDGE_TRANSFER) return e->minutes + half_headway(net, e->to);
    return e->minutes;
}

/* Bir kenarın ağırlıklı (çok kriterli) maliyeti. */
static double weighted_cost(const Network *net, const Edge *e, const RouteWeights *w)
{
    double cost = w->time * edge_cost(net, e);
    if (e->kind == EDGE_TRANSFER) cost += w->transfer;
    else cost += w->crowd * (net->stops[net->states[e->to].stop].crowd - 1);
    return cost;
}

/* from -> to arasındaki açık kenarlardan en ucuzunu bulur. */
static const Edge *find_edge(const Network *net, int from, int to)
{
    const State *st = &net->states[from];
    const Edge *best = NULL;
    for (int i = 0; i < st->edge_count; i++) {
        const Edge *e = &st->edges[i];
        if (e->to == to && !e->closed && (!best || edge_cost(net, e) < edge_cost(net, best))) best = e;
    }
    return best;
}

/* prev[] zincirini hedeften kaynağa izler. Zincir ters sırada geldiği için
 * önce stack'e itilir, sonra pop edilerek doğru sırada diziye yazılır. */
static void build_path(int target, const int *prev, Route *out)
{
    Stack st;
    stack_init(&st, sizeof(int));
    for (int s = target; s != -1; s = prev[s]) stack_push(&st, &s);

    out->state_count = (int)stack_size(&st);
    out->states = xmalloc(sizeof(int) * (size_t)out->state_count);
    for (int i = 0; i < out->state_count; i++) stack_pop(&st, &out->states[i]);
    stack_free(&st);
}

/* Rota boyunca süre/mesafe/aktarma özetini hesaplar. */
static void summarize(const Network *net, Route *r)
{
    r->ride_min = r->wait_min = r->walk_min = r->total_km = r->crowd_load = 0;
    r->transfers = 0;
    if (r->state_count == 0) return;
    if (r->state_count > 1) r->wait_min = half_headway(net, r->states[0]);   /* ilk biniş */

    for (int i = 1; i < r->state_count; i++) {
        const Edge *e = find_edge(net, r->states[i - 1], r->states[i]);
        if (!e) continue;
        if (e->kind == EDGE_RIDE) {
            r->ride_min += e->minutes;
            r->total_km += e->km;
            r->crowd_load += net->stops[net->states[e->to].stop].crowd - 1;
        } else {
            r->walk_min += e->minutes;
            r->wait_min += half_headway(net, e->to);
            r->transfers++;
        }
    }
    r->total_min = r->ride_min + r->wait_min + r->walk_min;

    /* Ortalama yoğunluk: aktarmada aynı durak iki kez sayılmasın */
    int stops = 0, last = -1;
    double crowd = 0;
    for (int i = 0; i < r->state_count; i++) {
        int s = net->states[r->states[i]].stop;
        if (s == last) continue;
        crowd += net->stops[s].crowd;
        stops++;
        last = s;
    }
    r->avg_crowd = stops ? crowd / stops : 0;
}

typedef enum { START_INVALID, START_DONE, START_SEARCH } StartResult;

/* Tüm rota fonksiyonlarının ortak başlangıcı: girdiyi doğrular, başlangıç ve
 * hedef aynıysa tek durumlu rotayı hazırlar. */
static StartResult route_start(const Network *net, int src_stop, int dst_stop, Route *out)
{
    memset(out, 0, sizeof *out);
    if (src_stop < 0 || dst_stop < 0 || src_stop >= net->stop_count || dst_stop >= net->stop_count)
        return START_INVALID;
    const Stop *src = &net->stops[src_stop];
    if (src->state_count == 0) return START_INVALID;

    if (src_stop == dst_stop) {   /* zaten oradayız */
        out->found = true;
        out->states = xmalloc(sizeof(int));
        out->states[0] = src->states[0];
        out->state_count = 1;
        return START_DONE;
    }
    return START_SEARCH;
}

bool route_fastest(const Network *net, int src_stop, int dst_stop, Route *out)
{
    return route_weighted(net, src_stop, dst_stop, &WEIGHTS_FASTEST, out);
}

bool route_weighted(const Network *net, int src_stop, int dst_stop,
                    const RouteWeights *w, Route *out)
{
    if (w->time < 0 || w->transfer < 0 || w->crowd < 0) {   /* Dijkstra şartı */
        memset(out, 0, sizeof *out);
        return false;
    }
    switch (route_start(net, src_stop, dst_stop, out)) {
    case START_INVALID: return false;
    case START_DONE:    return true;
    case START_SEARCH:  break;
    }
    const Stop *src = &net->stops[src_stop];

    int n = net->state_count;
    double *dist = xmalloc(sizeof(double) * (size_t)n);
    int *prev = xmalloc(sizeof(int) * (size_t)n);
    bool *done = xcalloc((size_t)n, sizeof(bool));
    for (int i = 0; i < n; i++) {
        dist[i] = INFINITY;
        prev[i] = -1;
    }

    MinHeap heap;
    heap_init(&heap, n);

    /* Başlangıç durağındaki her hat bir başlangıç noktasıdır */
    for (int i = 0; i < src->state_count; i++) {
        int s = src->states[i];
        dist[s] = w->time * half_headway(net, s);
        heap_push(&heap, s, dist[s]);
    }

    int target = -1, u;
    double d;
    while (heap_pop(&heap, &u, &d)) {
        done[u] = true;
        out->expanded++;
        if (net->states[u].stop == dst_stop) {   /* en kısa yol kesinleşti */
            target = u;
            break;
        }
        const State *st = &net->states[u];
        for (int i = 0; i < st->edge_count; i++) {
            const Edge *e = &st->edges[i];
            if (e->closed || done[e->to]) continue;
            double nd = d + weighted_cost(net, e, w);
            if (nd < dist[e->to]) {                /* gevşetme (relaxation) */
                dist[e->to] = nd;
                prev[e->to] = u;
                heap_push(&heap, e->to, nd);       /* ekle veya decrease-key */
            }
        }
    }

    if (target >= 0) {
        out->found = true;
        build_path(target, prev, out);
        summarize(net, out);
        out->cost = dist[target];
    }

    heap_free(&heap);
    free(dist);
    free(prev);
    free(done);
    return out->found;
}

bool route_astar(const Network *net, int src_stop, int dst_stop, Route *out)
{
    switch (route_start(net, src_stop, dst_stop, out)) {
    case START_INVALID: return false;
    case START_DONE:    return true;
    case START_SEARCH:  break;
    }
    const Stop *src = &net->stops[src_stop];

    int n = net->state_count;
    double *g = xmalloc(sizeof(double) * (size_t)n);          /* başlangıçtan gerçek maliyet */
    double *hcache = xmalloc(sizeof(double) * (size_t)net->stop_count);
    int *prev = xmalloc(sizeof(int) * (size_t)n);
    bool *done = xcalloc((size_t)n, sizeof(bool));
    for (int i = 0; i < n; i++) {
        g[i] = INFINITY;
        prev[i] = -1;
    }
    for (int i = 0; i < net->stop_count; i++) hcache[i] = -1;   /* tembel hesaplama */

    MinHeap heap;   /* öncelik f = g + h */
    heap_init(&heap, n);

#define H(stop)                                                                          \
    (hcache[stop] >= 0 ? hcache[stop]                                                    \
                       : (hcache[stop] = net->max_speed > 0                              \
                              ? network_stop_distance_km(net, stop, dst_stop) / net->max_speed \
                              : 0.0))

    for (int i = 0; i < src->state_count; i++) {
        int s = src->states[i];
        g[s] = half_headway(net, s);
        heap_push(&heap, s, g[s] + H(src_stop));
    }

    int target = -1, u;
    while (heap_pop(&heap, &u, NULL)) {
        done[u] = true;
        out->expanded++;
        if (net->states[u].stop == dst_stop) {
            target = u;
            break;
        }
        const State *st = &net->states[u];
        for (int i = 0; i < st->edge_count; i++) {
            const Edge *e = &st->edges[i];
            if (e->closed || done[e->to]) continue;
            double ng = g[u] + edge_cost(net, e);
            if (ng < g[e->to]) {
                g[e->to] = ng;
                prev[e->to] = u;
                heap_push(&heap, e->to, ng + H(net->states[e->to].stop));
            }
        }
    }
#undef H

    if (target >= 0) {
        out->found = true;
        build_path(target, prev, out);
        summarize(net, out);
        out->cost = g[target];
    }

    heap_free(&heap);
    free(g);
    free(hcache);
    free(prev);
    free(done);
    return out->found;
}

/* AŞAMA 1 — 0-1 BFS ile hedefe en az kaç aktarmayla gidilebileceğini bulur.
 * Ulaşılamıyorsa -1. */
static int min_transfer_count(const Network *net, int src_stop, int dst_stop, int *expanded)
{
    const Stop *src = &net->stops[src_stop];
    int n = net->state_count;
    int *transfers = xmalloc(sizeof(int) * (size_t)n);
    bool *done = xcalloc((size_t)n, sizeof(bool));
    for (int i = 0; i < n; i++) transfers[i] = -1;   /* -1: henüz ulaşılmadı */

    Queue dq;
    queue_init(&dq, sizeof(int));
    for (int i = 0; i < src->state_count; i++) {
        int s = src->states[i];
        transfers[s] = 0;
        queue_enqueue(&dq, &s);
    }

    int result = -1, u;
    while (queue_dequeue(&dq, &u)) {
        if (done[u]) continue;   /* bir durum kuyruğa birden fazla girebilir */
        done[u] = true;
        (*expanded)++;
        if (net->states[u].stop == dst_stop) {   /* kuyruk sıralı: ilk ulaşan en az aktarmalı */
            result = transfers[u];
            break;
        }
        const State *st = &net->states[u];
        for (int i = 0; i < st->edge_count; i++) {
            const Edge *e = &st->edges[i];
            if (e->closed || done[e->to]) continue;
            int w = e->kind == EDGE_TRANSFER ? 1 : 0;
            int nt = transfers[u] + w;
            if (transfers[e->to] == -1 || nt < transfers[e->to]) {
                transfers[e->to] = nt;
                if (w == 0) queue_push_front(&dq, &e->to);   /* aynı katman: öne */
                else        queue_enqueue(&dq, &e->to);      /* sonraki katman: arkaya */
            }
        }
    }

    queue_free(&dq);
    free(transfers);
    free(done);
    return result;
}

/* AŞAMA 2 — En fazla max_k aktarmayla gidilebilen rotalar arasından en
 * hızlısını bulur (kısıtlı en kısa yol).
 *
 * Grafı max_k+1 katmana kopyalarız: düğüm = (durum, kullanılan aktarma).
 * RIDE kenarı aynı katmanda kalır, TRANSFER kenarı bir üst katmana çıkar ve
 * max_k'yı aşan katman yoktur. Bu katmanlı grafta Dijkstra çalıştırılır.
 * Düğüm sayısı V*(max_k+1); bu ağda max_k en fazla 2-3 olduğundan küçüktür. */
static bool fastest_within(const Network *net, int src_stop, int dst_stop, int max_k, Route *out)
{
    const Stop *src = &net->stops[src_stop];
    int n = net->state_count;
    int nodes = n * (max_k + 1);              /* düğüm id = k * n + durum */
    double *dist = xmalloc(sizeof(double) * (size_t)nodes);
    int *prev = xmalloc(sizeof(int) * (size_t)nodes);
    bool *done = xcalloc((size_t)nodes, sizeof(bool));
    for (int i = 0; i < nodes; i++) {
        dist[i] = INFINITY;
        prev[i] = -1;
    }

    MinHeap heap;
    heap_init(&heap, nodes);
    for (int i = 0; i < src->state_count; i++) {
        int s = src->states[i];                  /* katman 0 */
        dist[s] = half_headway(net, s);
        heap_push(&heap, s, dist[s]);
    }

    int target = -1, u;
    double d;
    while (heap_pop(&heap, &u, &d)) {
        done[u] = true;
        out->expanded++;
        int k = u / n, s = u % n;
        if (net->states[s].stop == dst_stop) {
            target = u;
            break;
        }
        const State *st = &net->states[s];
        for (int i = 0; i < st->edge_count; i++) {
            const Edge *e = &st->edges[i];
            if (e->closed) continue;
            int nk = k + (e->kind == EDGE_TRANSFER);
            if (nk > max_k) continue;            /* aktarma hakkı bitti */
            int v = nk * n + e->to;
            if (done[v]) continue;
            double nd = d + edge_cost(net, e);
            if (nd < dist[v]) {
                dist[v] = nd;
                prev[v] = u;
                heap_push(&heap, v, nd);
            }
        }
    }

    if (target >= 0) {
        out->found = true;
        build_path(target, prev, out);
        for (int i = 0; i < out->state_count; i++) out->states[i] %= n;   /* düğüm -> durum */
        summarize(net, out);
        out->cost = dist[target];
    }

    heap_free(&heap);
    free(dist);
    free(prev);
    free(done);
    return out->found;
}

bool route_min_transfers(const Network *net, int src_stop, int dst_stop, Route *out)
{
    switch (route_start(net, src_stop, dst_stop, out)) {
    case START_INVALID: return false;
    case START_DONE:    return true;
    case START_SEARCH:  break;
    }

    int expanded = 0;
    int k = min_transfer_count(net, src_stop, dst_stop, &expanded);
    if (k < 0) return false;

    fastest_within(net, src_stop, dst_stop, k, out);
    out->expanded += expanded;
    return out->found;
}

void route_free(Route *r)
{
    free(r->states);
    memset(r, 0, sizeof *r);
}

/* ------------------------------------------------------------------ */
/* Yazdırma                                                           */
/* ------------------------------------------------------------------ */

void route_print(const Network *net, const Route *r, FILE *fp)
{
    if (!r->found || r->state_count == 0) {
        fprintf(fp, "Rota bulunamadi.\n");
        return;
    }
    const Stop *first = &net->stops[net->states[r->states[0]].stop];
    const Stop *last = &net->stops[net->states[r->states[r->state_count - 1]].stop];

    fprintf(fp, "\n%s -> %s\n", first->name, last->name);
    if (r->state_count == 1) {
        fprintf(fp, "Baslangic ve hedef ayni durak.\n");
        return;
    }
    fprintf(fp, "Toplam %.1f dk  (yolculuk %.1f + bekleme %.1f + yurume %.1f)  |  %d aktarma  |  %.1f km"
                "  |  ort. yogunluk %.1f/5\n\n",
            r->total_min, r->ride_min, r->wait_min, r->walk_min, r->transfers, r->total_km, r->avg_crowd);

    int i = 0;
    while (i < r->state_count - 1) {
        const Edge *e = find_edge(net, r->states[i], r->states[i + 1]);
        if (!e) break;

        if (e->kind == EDGE_TRANSFER) {
            const State *a = &net->states[r->states[i]], *b = &net->states[r->states[i + 1]];
            fprintf(fp, "   <> %s: %s -> %s aktarmasi (%.0f dk yurume + ~%.1f dk bekleme)\n",
                    net->stops[a->stop].name, net->lines[a->line].id, net->lines[b->line].id,
                    e->minutes, half_headway(net, r->states[i + 1]));
            i++;
            continue;
        }

        /* Aynı hattaki ardışık RIDE kenarlarını tek bir "biniş" olarak topla */
        int start = i;
        double minutes = 0;
        while (i < r->state_count - 1) {
            const Edge *x = find_edge(net, r->states[i], r->states[i + 1]);
            if (!x || x->kind != EDGE_RIDE) break;
            minutes += x->minutes;
            i++;
        }
        const State *s0 = &net->states[r->states[start]];
        fprintf(fp, "  [%s] %s -> %s  (%d istasyon, %.1f dk)\n", net->lines[s0->line].id,
                net->stops[s0->stop].name, net->stops[net->states[r->states[i]].stop].name,
                i - start, minutes);
        fprintf(fp, "       ");
        for (int k = start; k <= i; k++)
            fprintf(fp, "%s%s", k > start ? ", " : "", net->stops[net->states[r->states[k]].stop].name);
        fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
}
