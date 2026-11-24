/* Kent İçi Rota ve Hat Planlayıcı — komut satırı arayüzü.
 *
 * Kullanım:
 *   ./bin/rota                         etkileşimli menü (veri: ./data)
 *   ./bin/rota -d <klasor>             farklı veri klasörü
 *   ./bin/rota --route "Kadikoy" "Umraniye"       en hızlı rota, menüsüz
 *   ./bin/rota --transfers "Kadikoy" "Umraniye"   en az aktarmalı rota, menüsüz
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "graph.h"
#include "history.h"
#include "io_csv.h"
#include "route.h"
#include "sort.h"

#define MAX_MATCHES 20

/* ------------------------------------------------------------------ */
/* Girdi yardımcıları                                                 */
/* ------------------------------------------------------------------ */

/* Bir satır okur, sondaki satır sonunu siler. EOF'ta false. */
static bool read_line(const char *prompt, char *buf, size_t size)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)size, stdin)) return false;
    buf[strcspn(buf, "\r\n")] = '\0';
    return true;
}

static void print_stop(const Network *net, int idx)
{
    char lines[64];
    network_stop_lines(net, idx, lines, sizeof lines);
    printf("%s (%s)", net->stops[idx].name, lines);
}

/* Kullanıcıdan durak seçtirir. İptal/EOF için -1. */
static int ask_stop(const Network *net, const char *prompt)
{
    char buf[128];
    int matches[MAX_MATCHES];

    for (;;) {
        if (!read_line(prompt, buf, sizeof buf)) return -1;
        if (buf[0] == '\0') return -1;   /* boş giriş: menüye dön */

        int n = network_search_stops(net, buf, matches, MAX_MATCHES);
        if (n == 0) {
            printf("  '%s' ile eslesen durak yok, tekrar deneyin (bos birakirsaniz menuye doner).\n", buf);
            continue;
        }
        if (n == 1) {
            printf("  -> ");
            print_stop(net, matches[0]);
            printf("\n");
            return matches[0];
        }

        printf("  Birden fazla durak eslesti:\n");
        for (int i = 0; i < n; i++) {
            printf("    %2d) ", i + 1);
            print_stop(net, matches[i]);
            printf("\n");
        }
        if (!read_line("  Secim (numara): ", buf, sizeof buf)) return -1;
        int choice = atoi(buf);
        if (choice >= 1 && choice <= n) return matches[choice - 1];
        printf("  Gecersiz secim.\n");
    }
}

/* ------------------------------------------------------------------ */
/* Menü komutları                                                     */
/* ------------------------------------------------------------------ */

static void cmd_search(const Network *net)
{
    char buf[128];
    int matches[MAX_MATCHES];
    if (!read_line("Durak adi veya basi (or. 'koz', 'kadikoy'): ", buf, sizeof buf)) return;

    int n = network_search_stops(net, buf, matches, MAX_MATCHES);
    if (n == 0) {
        printf("Eslesen durak yok.\n");
        return;
    }
    for (int i = 0; i < n; i++) {
        const Stop *s = &net->stops[matches[i]];
        printf("  ");
        print_stop(net, matches[i]);
        if (s->connections[0]) printf("  | diger baglantilar: %s", s->connections);
        printf("\n");
    }
    if (n == MAX_MATCHES) printf("  (ilk %d sonuc gosterildi)\n", MAX_MATCHES);
}

typedef bool (*RouteFn)(const Network *, int, int, Route *);

static void cmd_route(const Network *net, RouteFn fn)
{
    int a = ask_stop(net, "Nereden: ");
    if (a < 0) return;
    int b = ask_stop(net, "Nereye : ");
    if (b < 0) return;

    Route r;
    fn(net, a, b, &r);
    route_print(net, &r, stdout);
    route_free(&r);
}

/* "m4", "M4 " gibi girdileri hat indeksine çevirir. İptal için -1. */
static int ask_line(const Network *net)
{
    char buf[32];
    for (;;) {
        printf("Hat (");
        for (int i = 0; i < net->line_count; i++) printf("%s%s", i ? "/" : "", net->lines[i].id);
        if (!read_line("): ", buf, sizeof buf) || buf[0] == '\0') return -1;

        char id[sizeof buf];
        size_t n = 0;
        for (const char *p = buf; *p; p++)
            if (!isspace((unsigned char)*p)) id[n++] = (char)toupper((unsigned char)*p);
        id[n] = '\0';

        int line = network_find_line(net, id);
        if (line >= 0) return line;
        printf("  '%s' diye bir hat yok.\n", buf);
    }
}

static void print_op(const Network *net, const char *prefix, const ClosureOp *op)
{
    printf("%s %s %s - %s %s\n", prefix, net->lines[op->line].id, net->stops[op->stop_a].name,
           net->stops[op->stop_b].name, op->closed ? "(kapali)" : "(acik)");
}

static void cmd_set_closed(Network *net, History *h, bool close)
{
    int line = ask_line(net);
    if (line < 0) return;
    int a = ask_stop(net, "1. istasyon: ");
    if (a < 0) return;
    int b = ask_stop(net, "2. istasyon: ");
    if (b < 0) return;

    switch (history_apply(h, net, a, b, line, close)) {
    case APPLY_OK:
        printf("%s: %s %s - %s\n", close ? "Kapatildi" : "Acildi", net->lines[line].id,
               net->stops[a].name, net->stops[b].name);
        break;
    case APPLY_NO_SEGMENT:
        printf("%s hattinda %s ile %s komsu istasyon degil.\n", net->lines[line].id,
               net->stops[a].name, net->stops[b].name);
        break;
    case APPLY_UNCHANGED:
        printf("Bu baglanti zaten %s.\n", close ? "kapali" : "acik");
        break;
    }
}

static void cmd_list_closed(const Network *net)
{
    int count = 0;
    for (int s = 0; s < net->state_count; s++) {
        const State *st = &net->states[s];
        for (int i = 0; i < st->edge_count; i++) {
            const Edge *e = &st->edges[i];
            if (e->kind != EDGE_RIDE || !e->closed || e->to < s) continue;   /* her bağlantı bir kez */
            printf("  %s %s - %s\n", net->lines[st->line].id, net->stops[st->stop].name,
                   net->stops[net->states[e->to].stop].name);
            count++;
        }
    }
    if (count == 0) printf("Kapali baglanti yok.\n");
}

static void cmd_undo(Network *net, History *h, bool redo)
{
    ClosureOp op;
    bool ok = redo ? history_redo(h, net, &op) : history_undo(h, net, &op);
    if (!ok) {
        printf("%s islem yok.\n", redo ? "Yinelenecek" : "Geri alinacak");
        return;
    }
    /* Geri almada bağlantı işlemin tersi duruma döner */
    ClosureOp now = op;
    if (!redo) now.closed = !op.closed;
    print_op(net, redo ? "Yinelendi:" : "Geri alindi:", &now);
}

/* Kullanıcıdan >= 0 bir sayı okur; boş giriş varsayılanı döndürür. */
static double ask_weight(const char *prompt, double def)
{
    char buf[32];
    for (;;) {
        printf("%s [%.1f]: ", prompt, def);
        if (!read_line("", buf, sizeof buf) || buf[0] == '\0') return def;
        char *end;
        double v = strtod(buf, &end);
        if (end != buf && *end == '\0' && v >= 0) return v;
        printf("  0 veya daha buyuk bir sayi girin.\n");
    }
}

static void cmd_weighted(const Network *net)
{
    char buf[16];
    RouteWeights w;
    printf("Tercih:\n"
           "  1) Dengeli            (aktarma cezasi 5 dk, yogunluk 0.5)\n"
           "  2) Kalabaliktan kacin (aktarma cezasi 2 dk, yogunluk 3)\n"
           "  3) Aktarmadan kacin   (aktarma cezasi 15 dk)\n"
           "  4) Ozel agirliklar\n");
    if (!read_line("Secim: ", buf, sizeof buf)) return;
    if (strcmp(buf, "1") == 0)      w = WEIGHTS_BALANCED;
    else if (strcmp(buf, "2") == 0) w = WEIGHTS_AVOID_CROWD;
    else if (strcmp(buf, "3") == 0) w = (RouteWeights){.time = 1, .transfer = 15, .crowd = 0};
    else if (strcmp(buf, "4") == 0) {
        w.time = ask_weight("  Dakika basina agirlik", 1.0);
        w.transfer = ask_weight("  Aktarma basina ceza (dk)", 5.0);
        w.crowd = ask_weight("  Yogunluk puani basina ceza (dk)", 0.5);
    } else {
        printf("Gecersiz secim.\n");
        return;
    }

    int a = ask_stop(net, "Nereden: ");
    if (a < 0) return;
    int b = ask_stop(net, "Nereye : ");
    if (b < 0) return;

    Route r, fast;
    route_weighted(net, a, b, &w, &r);
    route_print(net, &r, stdout);
    if (r.found && route_fastest(net, a, b, &fast) && r.state_count > 1) {
        double dt = r.total_min - fast.total_min;
        if (dt < 0.05)
            printf("Bu rota ayni zamanda en hizli rota.\n");
        else
            printf("En hizli rotaya gore %.1f dk daha uzun; aktarma %d -> %d, kalabalik yuku %.0f -> %.0f\n",
                   dt, fast.transfers, r.transfers, fast.crowd_load, r.crowd_load);
    }
    if (r.found) printf("Agirlikli maliyet: %.1f\n", r.cost);
    route_free(&r);
    route_free(&fast);
}

/* Listeleme için sıralama ölçütleri */
static const Network *g_sort_net;   /* karşılaştırma fonksiyonlarına ağı iletmek için */

static int cmp_stop_name(const void *a, const void *b)
{
    return strcmp(g_sort_net->stops[*(const int *)a].key, g_sort_net->stops[*(const int *)b].key);
}

static int cmp_stop_crowd_desc(const void *a, const void *b)
{
    int x = g_sort_net->stops[*(const int *)a].crowd, y = g_sort_net->stops[*(const int *)b].crowd;
    return (x < y) - (x > y);
}

/* Durakları yoğunluğa göre azalan, eşitlerde alfabetik listeler:
 * önce quicksort ile ada göre, sonra KARARLI mergesort ile yoğunluğa göre
 * sıralanır; mergesort eşit yoğunluktakilerin alfabetik sırasını korur. */
static void list_by_crowd(const Network *net, int limit)
{
    int *idx = malloc(sizeof(int) * (size_t)net->stop_count);
    if (!idx) return;
    for (int i = 0; i < net->stop_count; i++) idx[i] = i;
    g_sort_net = net;
    quick_sort(idx, (size_t)net->stop_count, sizeof(int), cmp_stop_name);
    merge_sort(idx, (size_t)net->stop_count, sizeof(int), cmp_stop_crowd_desc);

    for (int i = 0; i < net->stop_count && i < limit; i++) {
        const Stop *s = &net->stops[idx[i]];
        printf("  %d/5 %s %s", s->crowd, s->crowd > s->crowd_base ? "+" : s->crowd < s->crowd_base ? "-" : " ",
               s->name);
        printf("\n");
    }
    free(idx);
}

static void cmd_crowd(Network *net)
{
    char buf[16];
    printf("Yogunluk:\n"
           "  1) En kalabalik 15 durak\n"
           "  2) Bir duragin yogunlugunu ayarla\n"
           "  3) Canli yogunluk simulasyonu (rastgele)\n"
           "  4) Baslangic degerlerine don\n");
    if (!read_line("Secim: ", buf, sizeof buf)) return;

    if (strcmp(buf, "1") == 0) {
        printf("(+ / - : baslangic degerine gore artti / azaldi)\n");
        list_by_crowd(net, 15);
    } else if (strcmp(buf, "2") == 0) {
        int s = ask_stop(net, "Durak: ");
        if (s < 0) return;
        printf("Su anki yogunluk: %d/5\n", net->stops[s].crowd);
        if (!read_line("Yeni deger (1-5): ", buf, sizeof buf)) return;
        int v = atoi(buf);
        if (v < CROWD_MIN || v > CROWD_MAX) {
            printf("Deger 1-5 arasinda olmali.\n");
            return;
        }
        network_set_crowd(net, s, v);
        printf("%s: %d/5\n", net->stops[s].name, v);
    } else if (strcmp(buf, "3") == 0) {
        static unsigned seed = 2026;   /* her çağrıda farklı ama tekrarlanabilir */
        network_simulate_crowd(net, seed++);
        printf("Canli yogunluk guncellendi. En kalabalik duraklar:\n");
        list_by_crowd(net, 8);
    } else if (strcmp(buf, "4") == 0) {
        network_reset_crowd(net);
        printf("Yogunluklar baslangic degerlerine dondu.\n");
    } else {
        printf("Gecersiz secim.\n");
    }
}

static double now_sec(void)
{
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

/* Aynı sorguyu üç algoritmayla çalıştırıp süre ve açılan durum sayısını
 * karşılaştırır. Ağ küçük olduğu için her algoritma 2000 kez çalıştırılıp
 * ortalaması alınır. */
static void cmd_compare(const Network *net)
{
    int a = ask_stop(net, "Nereden: ");
    if (a < 0) return;
    int b = ask_stop(net, "Nereye : ");
    if (b < 0) return;

    enum { REPEAT = 2000 };
    struct { const char *name; RouteFn fn; } algos[] = {
        {"Dijkstra", route_fastest}, {"A*", route_astar}, {"En az aktarma", route_min_transfers},
    };
    printf("\n  %-14s %10s %8s %10s %14s\n", "Algoritma", "Sure (dk)", "Aktarma", "Acilan", "Sorgu suresi");
    for (size_t k = 0; k < sizeof algos / sizeof algos[0]; k++) {
        Route r;
        double t = now_sec();
        for (int i = 0; i < REPEAT; i++) {
            algos[k].fn(net, a, b, &r);
            if (i + 1 < REPEAT) route_free(&r);
        }
        double us = (now_sec() - t) * 1e6 / REPEAT;
        if (r.found)
            printf("  %-14s %10.1f %8d %10d %11.2f us\n", algos[k].name, r.total_min, r.transfers, r.expanded, us);
        else
            printf("  %-14s %10s\n", algos[k].name, "rota yok");
        route_free(&r);
    }
    printf("  (Acilan = heap/kuyruktan cikarilan durum sayisi; ag toplam %d durum)\n", net->state_count);
}

static void menu(Network *net)
{
    History hist;
    history_init(&hist);
    char buf[32];
    for (;;) {
        printf("\n=== Kent Ici Rota ve Hat Planlayici (M4 / M5 / M8) ===\n"
               "1) Durak ara\n"
               "2) En hizli rota\n"
               "3) En az aktarmali rota\n"
               "4) Cok kriterli rota (sure / aktarma / yogunluk)\n"
               "5) Yol kapat\n"
               "6) Yol ac\n"
               "7) Kapali yollari listele\n"
               "8) Geri al (undo)   [%zu]\n"
               "9) Yinele (redo)    [%zu]\n"
               "10) Yogunluk (listele / guncelle / simule et)\n"
               "11) Algoritma karsilastirmasi (Dijkstra / A* / en az aktarma)\n"
               "0) Cikis\n",
               stack_size(&hist.undo), stack_size(&hist.redo));
        if (!read_line("Secim: ", buf, sizeof buf)) break;

        if (strcmp(buf, "1") == 0)      cmd_search(net);
        else if (strcmp(buf, "2") == 0) cmd_route(net, route_fastest);
        else if (strcmp(buf, "3") == 0) cmd_route(net, route_min_transfers);
        else if (strcmp(buf, "4") == 0) cmd_weighted(net);
        else if (strcmp(buf, "5") == 0) cmd_set_closed(net, &hist, true);
        else if (strcmp(buf, "6") == 0) cmd_set_closed(net, &hist, false);
        else if (strcmp(buf, "7") == 0) cmd_list_closed(net);
        else if (strcmp(buf, "8") == 0) cmd_undo(net, &hist, false);
        else if (strcmp(buf, "9") == 0) cmd_undo(net, &hist, true);
        else if (strcmp(buf, "10") == 0) cmd_crowd(net);
        else if (strcmp(buf, "11") == 0) cmd_compare(net);
        else if (strcmp(buf, "0") == 0) break;
        else printf("Gecersiz secim.\n");
    }
    history_free(&hist);
}

/* ------------------------------------------------------------------ */
/* Tek sorgu modu (--route)                                           */
/* ------------------------------------------------------------------ */

static int resolve_or_report(const Network *net, const char *query)
{
    int matches[MAX_MATCHES];
    int n = network_search_stops(net, query, matches, MAX_MATCHES);
    if (n == 1) return matches[0];

    if (n == 0) {
        fprintf(stderr, "'%s' ile eslesen durak yok.\n", query);
    } else {
        fprintf(stderr, "'%s' birden fazla durakla eslesti:", query);
        for (int i = 0; i < n; i++) fprintf(stderr, "%s %s", i ? "," : "", net->stops[matches[i]].name);
        fprintf(stderr, "\n");
    }
    return -1;
}

static int run_single_route(const Network *net, RouteFn fn, const char *from, const char *to)
{
    int a = resolve_or_report(net, from);
    int b = resolve_or_report(net, to);
    if (a < 0 || b < 0) return EXIT_FAILURE;

    Route r;
    bool ok = fn(net, a, b, &r);
    route_print(net, &r, stdout);
    route_free(&r);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* ------------------------------------------------------------------ */

static void usage(const char *prog)
{
    fprintf(stderr,
            "Kullanim:\n"
            "  %s [-d veri_klasoru]\n"
            "  %s [-d veri_klasoru] --route \"Nereden\" \"Nereye\"      (en hizli)\n"
            "  %s [-d veri_klasoru] --transfers \"Nereden\" \"Nereye\"  (en az aktarma)\n",
            prog, prog, prog);
}

int main(int argc, char **argv)
{
    const char *data_dir = "data";
    const char *route_from = NULL, *route_to = NULL;
    RouteFn route_fn = route_fastest;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            data_dir = argv[++i];
        } else if ((strcmp(argv[i], "--route") == 0 || strcmp(argv[i], "--transfers") == 0) && i + 2 < argc) {
            route_fn = strcmp(argv[i], "--route") == 0 ? route_fastest : route_min_transfers;
            route_from = argv[++i];
            route_to = argv[++i];
        } else {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    Network net;
    network_init(&net);
    LoadReport rep;
    if (!network_load(&net, data_dir, stderr, &rep)) {
        fprintf(stderr, "Veri yuklenemedi ('%s' klasoru).\n", data_dir);
        network_free(&net);
        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;
    if (route_from) {
        status = run_single_route(&net, route_fn, route_from, route_to);
    } else {
        printf("%d durak, %d hat, %d baglanti yuklendi", net.stop_count, net.line_count, net.segment_count);
        if (rep.warnings) printf(" (%d satir atlandi)", rep.warnings);
        printf(".\n");
        menu(&net);
    }

    network_free(&net);
    return status;
}
