#include "graph.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "sort.h"
#include "text.h"

void network_init(Network *net)
{
    memset(net, 0, sizeof *net);
    hash_init(&net->stop_by_key, 64);
    hash_init(&net->line_by_id, 8);
    trie_init(&net->names);
}

void network_free(Network *net)
{
    for (int i = 0; i < net->state_count; i++) free(net->states[i].edges);
    free(net->states);
    free(net->stops);
    free(net->lines);
    hash_free(&net->stop_by_key);
    hash_free(&net->line_by_id);
    trie_free(&net->names);
    memset(net, 0, sizeof *net);
}

/* ------------------------------------------------------------------ */
/* Kurulum                                                            */
/* ------------------------------------------------------------------ */

int network_append_stop(Network *net, const Stop *s)
{
    if (net->stop_count == net->stop_cap) {
        net->stop_cap = net->stop_cap ? net->stop_cap * 2 : 64;
        net->stops = xrealloc(net->stops, sizeof(Stop) * (size_t)net->stop_cap);
    }
    Stop *dst = &net->stops[net->stop_count];
    *dst = *s;
    dst->state_count = 0;
    return net->stop_count++;
}

int network_append_line(Network *net, const Line *l)
{
    if (hash_get(&net->line_by_id, l->id, NULL)) return -1;
    if (net->line_count == net->line_cap) {
        net->line_cap = net->line_cap ? net->line_cap * 2 : 8;
        net->lines = xrealloc(net->lines, sizeof(Line) * (size_t)net->line_cap);
    }
    net->lines[net->line_count] = *l;
    hash_put(&net->line_by_id, l->id, net->line_count);
    return net->line_count++;
}

static int cmp_stop_id(const void *a, const void *b)
{
    int x = ((const Stop *)a)->id, y = ((const Stop *)b)->id;
    return (x > y) - (x < y);
}

bool network_index_stops(Network *net, FILE *log)
{
    /* id'ye göre sırala: kenarları yüklerken id -> indeks dönüşümü
     * binary search ile O(log n) yapılacak. */
    quick_sort(net->stops, (size_t)net->stop_count, sizeof(Stop), cmp_stop_id);

    for (int i = 1; i < net->stop_count; i++) {
        if (net->stops[i].id == net->stops[i - 1].id) {
            fprintf(log, "stops.csv: id %d birden fazla durakta kullanilmis\n", net->stops[i].id);
            return false;
        }
    }

    for (int i = 0; i < net->stop_count; i++) {
        Stop *s = &net->stops[i];
        text_normalize(s->name, s->key, sizeof s->key);
        if (s->key[0] == '\0') {
            fprintf(log, "stops.csv: id %d icin aranabilir ad yok\n", s->id);
            continue;
        }
        if (hash_get(&net->stop_by_key, s->key, NULL))
            fprintf(log, "stops.csv: '%s' adi tekrarlaniyor (id %d)\n", s->name, s->id);
        else
            hash_put(&net->stop_by_key, s->key, i);
        trie_insert(&net->names, s->key, i);
    }
    return true;
}

static void add_edge(Network *net, int from, int to, EdgeKind kind, double minutes, double km)
{
    State *st = &net->states[from];
    if (st->edge_count == st->edge_cap) {
        st->edge_cap = st->edge_cap ? st->edge_cap * 2 : 4;
        st->edges = xrealloc(st->edges, sizeof(Edge) * (size_t)st->edge_cap);
    }
    st->edges[st->edge_count++] = (Edge){.to = to, .minutes = minutes, .km = km,
                                         .kind = kind, .closed = false};
}

static int get_or_create_state(Network *net, int stop, int line)
{
    int existing = network_state(net, stop, line);
    if (existing >= 0) return existing;

    Stop *s = &net->stops[stop];
    if (s->state_count == MAX_LINES_PER_STOP) return -1;

    if (net->state_count == net->state_cap) {
        net->state_cap = net->state_cap ? net->state_cap * 2 : 64;
        net->states = xrealloc(net->states, sizeof(State) * (size_t)net->state_cap);
    }
    int id = net->state_count++;
    net->states[id] = (State){.stop = stop, .line = line, .edges = NULL,
                              .edge_count = 0, .edge_cap = 0};
    s->states[s->state_count++] = id;
    return id;
}

bool network_add_ride(Network *net, int stop_a, int stop_b, int line, double minutes, double km)
{
    int sa = get_or_create_state(net, stop_a, line);
    int sb = get_or_create_state(net, stop_b, line);
    if (sa < 0 || sb < 0) return false;
    add_edge(net, sa, sb, EDGE_RIDE, minutes, km);   /* iki yönlü */
    add_edge(net, sb, sa, EDGE_RIDE, minutes, km);
    net->segment_count++;

    /* A* sezgiseli için en yüksek hız. Durak koordinatlarından hesaplanır
     * (kenarın km alanından değil): böylece üçgen eşitsizliği sezgiselin
     * kullandığı mesafeyle aynı ölçüde geçerli olur. */
    double speed = network_stop_distance_km(net, stop_a, stop_b) / minutes;
    if (speed > net->max_speed) net->max_speed = speed;
    return true;
}

bool network_add_transfer(Network *net, int stop, int from_line, int to_line, double walk_min)
{
    int sa = network_state(net, stop, from_line);
    int sb = network_state(net, stop, to_line);
    if (sa < 0 || sb < 0 || sa == sb) return false;
    add_edge(net, sa, sb, EDGE_TRANSFER, walk_min, 0.0);
    return true;
}

/* ------------------------------------------------------------------ */
/* Coğrafya                                                           */
/* ------------------------------------------------------------------ */

double geo_distance_km(double lat1, double lon1, double lat2, double lon2)
{
    const double R = 6371.0, RAD = 3.14159265358979323846 / 180.0;
    double p1 = lat1 * RAD, p2 = lat2 * RAD;
    double dp = p2 - p1, dl = (lon2 - lon1) * RAD;
    double h = sin(dp / 2) * sin(dp / 2) + cos(p1) * cos(p2) * sin(dl / 2) * sin(dl / 2);
    return 2 * R * asin(sqrt(h));
}

double network_stop_distance_km(const Network *net, int stop_a, int stop_b)
{
    const Stop *a = &net->stops[stop_a], *b = &net->stops[stop_b];
    return geo_distance_km(a->lat, a->lon, b->lat, b->lon);
}

/* ------------------------------------------------------------------ */
/* Sorgular                                                           */
/* ------------------------------------------------------------------ */

int network_find_stop_by_id(const Network *net, int csv_id)
{
    Stop key;
    key.id = csv_id;
    return (int)binary_search(net->stops, (size_t)net->stop_count, sizeof(Stop), &key, cmp_stop_id);
}

int network_find_line(const Network *net, const char *line_id)
{
    int idx;
    return hash_get(&net->line_by_id, line_id, &idx) ? idx : -1;
}

int network_state(const Network *net, int stop, int line)
{
    const Stop *s = &net->stops[stop];
    for (int i = 0; i < s->state_count; i++)
        if (net->states[s->states[i]].line == line) return s->states[i];
    return -1;
}

int network_search_stops(const Network *net, const char *query, int *out, int max)
{
    char key[NAME_MAX_LEN];
    text_normalize(query, key, sizeof key);
    if (key[0] == '\0' || max <= 0) return 0;

    int idx;
    if (hash_get(&net->stop_by_key, key, &idx)) {
        out[0] = idx;
        return 1;
    }
    return trie_prefix(&net->names, key, out, max);
}

void network_stop_lines(const Network *net, int stop, char *buf, size_t size)
{
    const Stop *s = &net->stops[stop];
    size_t len = 0;
    buf[0] = '\0';
    for (int i = 0; i < s->state_count && len < size; i++) {
        const char *id = net->lines[net->states[s->states[i]].line].id;
        int n = snprintf(buf + len, size - len, "%s%s", i ? ", " : "", id);
        if (n < 0) break;
        len += (size_t)n;
    }
}

static bool set_closed(Network *net, int from, int to, bool closed)
{
    State *st = &net->states[from];
    for (int i = 0; i < st->edge_count; i++) {
        if (st->edges[i].to == to && st->edges[i].kind == EDGE_RIDE) {
            st->edges[i].closed = closed;
            return true;
        }
    }
    return false;
}

bool network_set_segment_closed(Network *net, int stop_a, int stop_b, int line, bool closed)
{
    int sa = network_state(net, stop_a, line);
    int sb = network_state(net, stop_b, line);
    if (sa < 0 || sb < 0) return false;
    bool ok = set_closed(net, sa, sb, closed);
    ok = set_closed(net, sb, sa, closed) && ok;
    return ok;
}

int network_segment_status(const Network *net, int stop_a, int stop_b, int line)
{
    int sa = network_state(net, stop_a, line);
    int sb = network_state(net, stop_b, line);
    if (sa < 0 || sb < 0) return -1;
    const State *st = &net->states[sa];
    for (int i = 0; i < st->edge_count; i++)
        if (st->edges[i].to == sb && st->edges[i].kind == EDGE_RIDE) return st->edges[i].closed ? 1 : 0;
    return -1;
}

/* ------------------------------------------------------------------ */
/* Canlı yoğunluk                                                     */
/* ------------------------------------------------------------------ */

static int clamp_crowd(int v)
{
    return v < CROWD_MIN ? CROWD_MIN : v > CROWD_MAX ? CROWD_MAX : v;
}

void network_set_crowd(Network *net, int stop, int crowd)
{
    if (stop >= 0 && stop < net->stop_count) net->stops[stop].crowd = clamp_crowd(crowd);
}

void network_reset_crowd(Network *net)
{
    for (int i = 0; i < net->stop_count; i++) net->stops[i].crowd = net->stops[i].crowd_base;
}

void network_simulate_crowd(Network *net, unsigned seed)
{
    /* Basit doğrusal eşlik üreteci (LCG): rand()'dan bağımsız, her
     * platformda aynı diziyi üretir -> tekrarlanabilir demo ve test. */
    unsigned x = seed ? seed : 1u;
    for (int i = 0; i < net->stop_count; i++) {
        x = x * 1103515245u + 12345u;
        int delta = (int)((x >> 16) % 4u) - 1;   /* -1, 0, +1, +2 */
        net->stops[i].crowd = clamp_crowd(net->stops[i].crowd_base + delta);
    }
}
