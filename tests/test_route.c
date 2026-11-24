/* Graf, CSV yükleme ve Dijkstra testleri: make test (proje kökünden çalışır) */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gen.h"
#include "graph.h"
#include "history.h"
#include "io_csv.h"
#include "route.h"

static int g_checks = 0, g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        g_checks++;                                                          \
        if (!(cond)) {                                                       \
            g_failures++;                                                    \
            fprintf(stderr, "  BASARISIZ %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                    \
    } while (0)

#define NEAR(a, b) (fabs((a) - (b)) < 0.05)

static Network g_net;

static int stop(const char *name)
{
    int out[4];
    int n = network_search_stops(&g_net, name, out, 4);
    if (n != 1) {
        fprintf(stderr, "  durak bulunamadi/belirsiz: %s\n", name);
        exit(EXIT_FAILURE);
    }
    return out[0];
}

static int line(const char *id)
{
    return network_find_line(&g_net, id);
}

/* Rotanın uğradığı durakların adlarını "A>B>C" biçiminde birleştirir
 * (aktarmada aynı durak iki kez görünmesin diye tekrarlar atlanır). */
static void stops_of(const Route *r, char *buf, size_t size)
{
    buf[0] = '\0';
    int last = -1;
    for (int i = 0; i < r->state_count; i++) {
        int s = g_net.states[r->states[i]].stop;
        if (s == last) continue;
        if (last != -1) strncat(buf, ">", size - strlen(buf) - 1);
        strncat(buf, g_net.stops[s].name, size - strlen(buf) - 1);
        last = s;
    }
}

static bool route_passes(const Route *r, const char *name)
{
    int s = stop(name);
    for (int i = 0; i < r->state_count; i++)
        if (g_net.states[r->states[i]].stop == s) return true;
    return false;
}

/* ---------------------------------------------------------------- */

static void test_load(void)
{
    CHECK(g_net.stop_count == 57);
    CHECK(g_net.line_count == 3);
    CHECK(g_net.segment_count == 57);
    CHECK(g_net.state_count == 23 + 24 + 13);   /* her hattın istasyonları */

    /* binary search ile id -> durak */
    int k = network_find_stop_by_id(&g_net, 1);
    CHECK(k >= 0 && strcmp(g_net.stops[k].name, "Kadıköy") == 0);
    CHECK(network_find_stop_by_id(&g_net, 999) == -1);

    /* Aktarma istasyonlarında iki durum olmalı */
    CHECK(g_net.stops[stop("Kozyatağı")].state_count == 2);
    CHECK(g_net.stops[stop("Dudullu")].state_count == 2);
    CHECK(g_net.stops[stop("Bostancı")].state_count == 2);
    CHECK(g_net.stops[stop("Kadıköy")].state_count == 1);

    char buf[32];
    network_stop_lines(&g_net, stop("Kozyatağı"), buf, sizeof buf);
    CHECK(strcmp(buf, "M4, M8") == 0);
}

static void test_search(void)
{
    int out[20];
    /* Türkçe karakter olmadan tam eşleşme (hash) */
    CHECK(network_search_stops(&g_net, "kadikoy", out, 20) == 1);
    CHECK(network_search_stops(&g_net, "USKUDAR", out, 20) == 1);
    /* Önek araması (trie): "kuc" -> Küçükbakkalköy, Küçükyalı */
    CHECK(network_search_stops(&g_net, "kuc", out, 20) == 2);
    /* "sancaktepe" tam eşleşir, "Sancaktepe Şehir Hastanesi" ayrıca önekle bulunur */
    CHECK(network_search_stops(&g_net, "sancaktepe", out, 20) == 1);
    CHECK(network_search_stops(&g_net, "sancaktepe s", out, 20) == 1);
    CHECK(network_search_stops(&g_net, "taksim", out, 20) == 0);
    CHECK(network_search_stops(&g_net, "  ", out, 20) == 0);
}

static void test_fastest(void)
{
    Route r;
    char path[1024];

    /* Değerler veri hazırlanırken Python ile bağımsız hesaplanan sonuçlarla aynı */
    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Ümraniye"), &r));
    CHECK(NEAR(r.total_min, 60.7));
    CHECK(r.transfers == 2);
    CHECK(route_passes(&r, "Kozyatağı") && route_passes(&r, "Dudullu"));
    CHECK(!route_passes(&r, "Bostancı"));   /* 20 dk'lık yürüme aktarması seçilmemeli */
    CHECK(NEAR(r.total_min, r.ride_min + r.wait_min + r.walk_min));
    route_free(&r);

    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Parseller"), &r));
    CHECK(NEAR(r.total_min, 44.2) && r.transfers == 1);
    route_free(&r);

    CHECK(route_fastest(&g_net, stop("Üsküdar"), stop("Sabiha Gökçen Havalimanı"), &r));
    CHECK(NEAR(r.total_min, 98.7) && r.transfers == 2);
    route_free(&r);

    /* Tek hat, aktarmasız: M4 Kadıköy -> Kartal */
    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Kartal"), &r));
    CHECK(r.transfers == 0 && NEAR(r.walk_min, 0.0));
    stops_of(&r, path, sizeof path);
    CHECK(strncmp(path, "Kadıköy>Ayrılık Çeşmesi>Acıbadem", strlen("Kadıköy>Ayrılık Çeşmesi>Acıbadem")) == 0);
    route_free(&r);

    /* Simetri: A->B ve B->A aynı süre (kenarlar iki yönlü, bekleme hat bazlı) */
    Route back;
    route_fastest(&g_net, stop("Göztepe"), stop("İçerenköy"), &r);
    route_fastest(&g_net, stop("İçerenköy"), stop("Göztepe"), &back);
    CHECK(r.found && back.found && r.transfers == back.transfers);
    route_free(&r);
    route_free(&back);

    /* Aynı durak */
    CHECK(route_fastest(&g_net, stop("Kartal"), stop("Kartal"), &r));
    CHECK(r.state_count == 1 && r.total_min == 0.0);
    route_free(&r);

    /* Geçersiz girdi */
    CHECK(!route_fastest(&g_net, -1, stop("Kartal"), &r));
    route_free(&r);
}

static void test_closures(void)
{
    Route r;
    int m8 = line("M8"), m4 = line("M4");
    int koz = stop("Kozyatağı"), ayse = stop("Ayşekadın"), kbk = stop("Küçükbakkalköy");

    /* Normalde Kadıköy -> Emin Ali Paşa: M4 ile Kozyatağı, M8 ile güneye */
    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Emin Ali Paşa"), &r));
    double normal = r.total_min;
    CHECK(route_passes(&r, "Kozyatağı") && !route_passes(&r, "Bostancı"));
    route_free(&r);

    /* Kozyatağı–Ayşekadın (M8) kapanınca tek yol Bostancı'daki 20 dk'lık
     * yürüme aktarması: rota değişmeli ve uzamalı */
    CHECK(network_set_segment_closed(&g_net, koz, ayse, m8, true));
    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Emin Ali Paşa"), &r));
    CHECK(r.found && r.total_min > normal + 10 && route_passes(&r, "Bostancı"));
    CHECK(r.transfers == 1 && NEAR(r.walk_min, 20.0));
    route_free(&r);

    /* Kozyatağı–Küçükbakkalköy da kapanınca M8'in kuzeyi (ve dolayısıyla
     * M5) Kadıköy'den ulaşılamaz olur */
    CHECK(network_set_segment_closed(&g_net, koz, kbk, m8, true));
    CHECK(!route_fastest(&g_net, stop("Kadıköy"), stop("Ümraniye"), &r));
    route_free(&r);
    /* Kapalı bağlantının iki ucu kendi taraflarında hâlâ erişilebilir */
    CHECK(route_fastest(&g_net, stop("Parseller"), stop("Üsküdar"), &r));
    route_free(&r);

    /* Geri açınca eski sonuçlar */
    CHECK(network_set_segment_closed(&g_net, koz, ayse, m8, false));
    CHECK(network_set_segment_closed(&g_net, koz, kbk, m8, false));
    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Emin Ali Paşa"), &r));
    CHECK(NEAR(r.total_min, normal));
    route_free(&r);
    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Ümraniye"), &r));
    CHECK(NEAR(r.total_min, 60.7));
    route_free(&r);

    /* Komşu olmayan istasyonlar arasında bağlantı yok */
    CHECK(!network_set_segment_closed(&g_net, stop("Kadıköy"), stop("Kartal"), m4, true));
    /* Durak o hatta değil */
    CHECK(!network_set_segment_closed(&g_net, stop("Kadıköy"), stop("Üsküdar"), m8, true));
}

static void test_min_transfers(void)
{
    Route r;
    CHECK(route_min_transfers(&g_net, stop("Kadıköy"), stop("Ümraniye"), &r));
    CHECK(r.transfers == 2 && NEAR(r.total_min, 60.7));
    route_free(&r);

    /* Aynı aktarma sayısında hızlı olan seçilmeli: Bostancı'da 20 dk yürümek
     * yerine Kozyatağı aktarması */
    CHECK(route_min_transfers(&g_net, stop("Kartal"), stop("Emin Ali Paşa"), &r));
    CHECK(r.transfers == 1 && route_passes(&r, "Kozyatağı") && NEAR(r.total_min, 37.2));
    route_free(&r);

    CHECK(route_min_transfers(&g_net, stop("Kadıköy"), stop("Kartal"), &r));
    CHECK(r.transfers == 0);
    route_free(&r);

    CHECK(route_min_transfers(&g_net, stop("Kartal"), stop("Kartal"), &r) && r.state_count == 1);
    route_free(&r);

    /* Tüm durak çiftleri için tutarlılık (57 x 57 = 3249 sorgu):
     *  - iki algoritma da aynı çiftlerde rota bulmalı
     *  - BFS'in aktarma sayısı en hızlı rotanınkinden fazla olamaz
     *  - BFS rotası en hızlı rotadan kısa olamaz
     *  - aktarma sayıları eşitse süreler de eşit olmalı (ikisi de o kümede en hızlı) */
    int bad = 0, fewer = 0;
    for (int a = 0; a < g_net.stop_count; a++) {
        for (int b = 0; b < g_net.stop_count; b++) {
            Route f, m;
            bool ff = route_fastest(&g_net, a, b, &f);
            bool mf = route_min_transfers(&g_net, a, b, &m);
            if (ff != mf || m.transfers > f.transfers || m.total_min < f.total_min - 1e-9 ||
                (m.transfers == f.transfers && !NEAR(m.total_min, f.total_min)))
                bad++;
            if (m.transfers < f.transfers) fewer++;
            route_free(&f);
            route_free(&m);
        }
    }
    CHECK(bad == 0);
    printf("          (%d cift icin en az aktarmali rota, en hizli rotadan daha az aktarma kullaniyor)\n", fewer);
}

static void test_history(void)
{
    History h;
    history_init(&h);
    int m8 = line("M8"), m4 = line("M4");
    int koz = stop("Kozyatağı"), ayse = stop("Ayşekadın"), kbk = stop("Küçükbakkalköy");
    ClosureOp op;

    CHECK(!history_undo(&h, &g_net, &op));   /* boş geçmiş */
    CHECK(!history_redo(&h, &g_net, &op));

    CHECK(history_apply(&h, &g_net, koz, ayse, m8, true) == APPLY_OK);
    CHECK(network_segment_status(&g_net, koz, ayse, m8) == 1);
    CHECK(history_apply(&h, &g_net, ayse, koz, m8, true) == APPLY_UNCHANGED);   /* ters yön de aynı bağlantı */
    CHECK(history_apply(&h, &g_net, stop("Kadıköy"), stop("Kartal"), m4, true) == APPLY_NO_SEGMENT);
    CHECK(history_apply(&h, &g_net, koz, kbk, m8, true) == APPLY_OK);
    CHECK(stack_size(&h.undo) == 2);   /* başarısız denemeler geçmişe yazılmaz */

    /* Undo: en son yapılan ilk geri alınır (LIFO) */
    CHECK(history_undo(&h, &g_net, &op) && op.stop_b == kbk);
    CHECK(network_segment_status(&g_net, koz, kbk, m8) == 0);
    CHECK(network_segment_status(&g_net, koz, ayse, m8) == 1);
    CHECK(history_undo(&h, &g_net, &op) && op.stop_b == ayse);
    CHECK(network_segment_status(&g_net, koz, ayse, m8) == 0);
    CHECK(!history_undo(&h, &g_net, &op));

    /* Redo: geri alınanlar ters sırayla yeniden uygulanır */
    CHECK(history_redo(&h, &g_net, &op) && op.stop_b == ayse);
    CHECK(network_segment_status(&g_net, koz, ayse, m8) == 1);
    CHECK(stack_size(&h.redo) == 1);

    /* Yeni işlem redo geçmişini siler */
    CHECK(history_apply(&h, &g_net, koz, ayse, m8, false) == APPLY_OK);   /* açma işlemi */
    CHECK(stack_size(&h.redo) == 0);
    CHECK(!history_redo(&h, &g_net, &op));

    /* Açma işlemini geri almak bağlantıyı yeniden kapatır */
    CHECK(history_undo(&h, &g_net, &op) && !op.closed);
    CHECK(network_segment_status(&g_net, koz, ayse, m8) == 1);

    /* Temizlik: hepsini geri al, ağ başlangıç haline dönmeli */
    while (history_undo(&h, &g_net, NULL)) {}
    CHECK(network_segment_status(&g_net, koz, ayse, m8) == 0);
    CHECK(network_segment_status(&g_net, koz, kbk, m8) == 0);
    Route r;
    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Ümraniye"), &r) && NEAR(r.total_min, 60.7));
    route_free(&r);

    history_free(&h);
}

static void test_weighted(void)
{
    Route r, f;

    /* WEIGHTS_FASTEST ile tüm çiftlerde route_fastest ile aynı sonuç; maliyet = süre */
    int bad = 0;
    for (int a = 0; a < g_net.stop_count; a += 3) {
        for (int b = 0; b < g_net.stop_count; b++) {
            route_weighted(&g_net, a, b, &WEIGHTS_FASTEST, &r);
            route_fastest(&g_net, a, b, &f);
            if (r.found != f.found || !NEAR(r.total_min, f.total_min) ||
                (r.state_count > 1 && !NEAR(r.cost, r.total_min)))
                bad++;
            route_free(&r);
            route_free(&f);
        }
    }
    CHECK(bad == 0);

    /* Kalabalıktan kaçınınca Kartal -> Emin Ali Paşa, Kozyatağı (4/5) ve
     * Ayşekadın'a girmemek için Bostancı yürüme aktarmasını seçer */
    CHECK(route_weighted(&g_net, stop("Kartal"), stop("Emin Ali Paşa"), &WEIGHTS_AVOID_CROWD, &r));
    route_fastest(&g_net, stop("Kartal"), stop("Emin Ali Paşa"), &f);
    CHECK(route_passes(&r, "Bostancı") && !route_passes(&r, "Ayşekadın"));
    CHECK(r.crowd_load < f.crowd_load && r.total_min > f.total_min);
    route_free(&r);
    route_free(&f);

    /* Dengeli ağırlıkla en hızlı rota korunur (Kozyatağı aktarması) */
    CHECK(route_weighted(&g_net, stop("Kartal"), stop("Emin Ali Paşa"), &WEIGHTS_BALANCED, &r));
    CHECK(route_passes(&r, "Ayşekadın") && NEAR(r.total_min, 37.2));
    route_free(&r);

    /* Yoğunluk metrikleri: Kadıköy (5) -> Ayrılık Çeşmesi (4) */
    CHECK(route_fastest(&g_net, stop("Kadıköy"), stop("Ayrılık Çeşmesi"), &r));
    CHECK(NEAR(r.avg_crowd, 4.5) && NEAR(r.crowd_load, 3.0));
    route_free(&r);

    /* Negatif ağırlık reddedilir */
    RouteWeights neg = {1, -1, 0};
    CHECK(!route_weighted(&g_net, stop("Kadıköy"), stop("Kartal"), &neg, &r));
    route_free(&r);
}

static void test_crowd(void)
{
    int koz = stop("Kozyatağı");
    CHECK(g_net.stops[koz].crowd == 4 && g_net.stops[koz].crowd_base == 4);

    network_set_crowd(&g_net, koz, 9);   /* aralık dışı -> 5'e sıkıştırılır */
    CHECK(g_net.stops[koz].crowd == 5);
    network_set_crowd(&g_net, koz, 0);
    CHECK(g_net.stops[koz].crowd == 1);
    network_set_crowd(&g_net, -1, 3);    /* geçersiz durak: çökmemeli */

    /* Canlı yoğunluk Kozyatağı'nı boşaltınca kalabalıktan kaçınan rota da
     * Kozyatağı aktarmasına döner */
    Route r;
    route_weighted(&g_net, stop("Kartal"), stop("Emin Ali Paşa"), &WEIGHTS_AVOID_CROWD, &r);
    CHECK(route_passes(&r, "Ayşekadın"));
    route_free(&r);

    /* Simülasyon: değerler aralıkta, temel değerden en fazla -1/+2 sapma,
     * aynı seed aynı sonuç */
    network_simulate_crowd(&g_net, 7);
    int ok = 1, changed = 0, first[64];
    for (int i = 0; i < g_net.stop_count; i++) {
        const Stop *s = &g_net.stops[i];
        int d = s->crowd - s->crowd_base;
        if (s->crowd < 1 || s->crowd > 5 || d < -1 || d > 2) ok = 0;
        if (d) changed++;
        first[i] = s->crowd;
    }
    CHECK(ok && changed > 0);
    network_simulate_crowd(&g_net, 7);
    for (int i = 0; i < g_net.stop_count; i++)
        if (g_net.stops[i].crowd != first[i]) ok = 0;
    CHECK(ok);

    network_reset_crowd(&g_net);
    for (int i = 0; i < g_net.stop_count; i++)
        if (g_net.stops[i].crowd != g_net.stops[i].crowd_base) ok = 0;
    CHECK(ok);
}

static void test_astar(void)
{
    /* Tüm çiftlerde A* = Dijkstra (süre), A* toplamda daha az durum açar,
     * sezgisel hiçbir zaman gerçek süreyi aşmaz (kabul edilebilirlik) */
    int bad = 0, inadmissible = 0;
    long exp_d = 0, exp_a = 0;
    for (int a = 0; a < g_net.stop_count; a++) {
        for (int b = 0; b < g_net.stop_count; b++) {
            Route d, s;
            route_fastest(&g_net, a, b, &d);
            route_astar(&g_net, a, b, &s);
            if (d.found != s.found || !NEAR(d.total_min, s.total_min)) bad++;
            double h = network_stop_distance_km(&g_net, a, b) / g_net.max_speed;
            if (d.found && h > d.total_min + 1e-9) inadmissible++;
            exp_d += d.expanded;
            exp_a += s.expanded;
            route_free(&d);
            route_free(&s);
        }
    }
    CHECK(bad == 0);
    CHECK(inadmissible == 0);
    CHECK(exp_a < exp_d);
    CHECK(g_net.max_speed > 0);

    /* Kapalı yolla da aynı sonuç */
    int koz = stop("Kozyatağı"), ayse = stop("Ayşekadın"), m8 = line("M8");
    network_set_segment_closed(&g_net, koz, ayse, m8, true);
    Route d, s;
    route_fastest(&g_net, stop("Kadıköy"), stop("Emin Ali Paşa"), &d);
    route_astar(&g_net, stop("Kadıköy"), stop("Emin Ali Paşa"), &s);
    CHECK(d.found && s.found && NEAR(d.total_min, s.total_min) && route_passes(&s, "Bostancı"));
    route_free(&d);
    route_free(&s);
    network_set_segment_closed(&g_net, koz, ayse, m8, false);

    /* Haversine: Kadıköy - Üsküdar arası yaklaşık 4 km */
    double km = network_stop_distance_km(&g_net, stop("Kadıköy"), stop("Üsküdar"));
    CHECK(km > 3.5 && km < 4.5);
}

static void test_grid(void)
{
    Network net;
    network_init(&net);
    CHECK(gen_grid_city(&net, 12, 7));
    CHECK(net.stop_count == 144 && net.state_count == 288 && net.line_count == 24);
    CHECK(net.segment_count == 2 * 12 * 11);

    /* Rastgele çiftlerde üç algoritma tutarlı; ızgarada döngüler olduğu
     * için en az aktarmalı rota bazen en hızlıdan daha az aktarma kullanır */
    unsigned x = 99;
    int bad = 0, fewer = 0;
    long exp_d = 0, exp_a = 0;
    for (int i = 0; i < 400; i++) {
        x = x * 1103515245u + 12345u;
        int a = (int)((x >> 16) % 144u);
        x = x * 1103515245u + 12345u;
        int b = (int)((x >> 16) % 144u);
        Route d, s, m;
        route_fastest(&net, a, b, &d);
        route_astar(&net, a, b, &s);
        route_min_transfers(&net, a, b, &m);
        if (!d.found || !s.found || !m.found || !NEAR(d.total_min, s.total_min) ||
            m.transfers > d.transfers || m.total_min < d.total_min - 1e-9 || m.transfers > 1)
            bad++;   /* ızgarada her yere en fazla 1 aktarmayla gidilir */
        if (m.transfers < d.transfers) fewer++;
        exp_d += d.expanded;
        exp_a += s.expanded;
        route_free(&d);
        route_free(&s);
        route_free(&m);
    }
    CHECK(bad == 0);
    CHECK(fewer > 0);
    CHECK(exp_a * 2 < exp_d);   /* ızgarada A* belirgin şekilde daha az durum açar */

    /* Aynı seed aynı ağ */
    Network again;
    network_init(&again);
    gen_grid_city(&again, 12, 7);
    CHECK(again.stops[50].lat == net.stops[50].lat && again.lines[3].headway_min == net.lines[3].headway_min);
    network_free(&again);

    CHECK(!gen_grid_city(&net, 1, 7));   /* geçersiz boyut */
    network_free(&net);
}

static void test_bad_data(void)
{
    /* Hatalı satırlar içeren geçici veri klasörü */
    const char *dir = "build/testdata";
    char path[256];
    system("mkdir -p build/testdata");

    snprintf(path, sizeof path, "%s/stops.csv", dir);
    FILE *f = fopen(path, "w");
    fprintf(f, "id,name,lat,lon,crowd,connections\n"
               "1,A Durağı,41.0,29.0,3,\n"
               "2,B Durağı,41.0,29.1,9,\n"         /* yoğunluk aralık dışı -> uyarı, 3 */
               "x,Bozuk,41.0,29.0,3,\n"            /* geçersiz id */
               "3,C Durağı,abc,29.2,3,\n"          /* geçersiz koordinat */
               "\n"
               "4,D Durağı,41.0,29.3,2\n");        /* bağlantı alanı yok: 5 alan geçerli */
    fclose(f);

    snprintf(path, sizeof path, "%s/lines.csv", dir);
    f = fopen(path, "w");
    fprintf(f, "line_id,name,type,color,headway_min,total_min,air_km\n"
               "L1,A - D,METRO,#000000,4,10,5\n"
               "L1,tekrar,METRO,#000000,4,10,5\n"  /* tekrar eden hat */
               "L2,eksik alan\n");                 /* alan sayısı yanlış */
    fclose(f);

    snprintf(path, sizeof path, "%s/edges.csv", dir);
    f = fopen(path, "w");
    fprintf(f, "from_id,to_id,line_id,minutes,distance_km\n"
               "1,2,L1,3.0,1.0\n"
               "2,4,L1,2.5,1.0\n"
               "2,99,L1,2.0,1.0\n"                 /* bilinmeyen durak */
               "1,2,L9,2.0,1.0\n"                  /* bilinmeyen hat */
               "1,4,L1,-1,1.0\n");                 /* negatif süre */
    fclose(f);

    snprintf(path, sizeof path, "%s/transfers.csv", dir);
    f = fopen(path, "w");
    fprintf(f, "stop_id,from_line,to_line,walk_min\n");
    fclose(f);

    Network net;
    network_init(&net);
    LoadReport rep;
    FILE *devnull = fopen("/dev/null", "w");
    CHECK(network_load(&net, dir, devnull, &rep));
    CHECK(net.stop_count == 3 && net.line_count == 1 && net.segment_count == 2);
    CHECK(rep.warnings == 8);
    CHECK(net.stops[network_find_stop_by_id(&net, 2)].crowd == 3);

    Route r;
    CHECK(route_fastest(&net, network_find_stop_by_id(&net, 1), network_find_stop_by_id(&net, 4), &r));
    CHECK(NEAR(r.total_min, 2.0 + 3.0 + 2.5));   /* bekleme 4/2 + 3 + 2.5 */
    route_free(&r);
    network_free(&net);

    /* Olmayan klasör: yükleme başarısız olmalı */
    network_init(&net);
    CHECK(!network_load(&net, "build/yok", devnull, &rep));
    network_free(&net);
    fclose(devnull);
}

int main(void)
{
    network_init(&g_net);
    LoadReport rep;
    if (!network_load(&g_net, "data", stderr, &rep)) {
        fprintf(stderr, "data/ yuklenemedi; testleri proje kokunden calistirin.\n");
        return EXIT_FAILURE;
    }
    CHECK(rep.warnings == 0);

    struct { const char *name; void (*fn)(void); } tests[] = {
        {"load", test_load},       {"search", test_search},     {"fastest", test_fastest},
        {"closures", test_closures}, {"transfers", test_min_transfers},
        {"history", test_history},   {"weighted", test_weighted},
        {"crowd", test_crowd},       {"astar", test_astar},
        {"grid", test_grid},         {"bad data", test_bad_data},
    };
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; i++) {
        int before = g_failures;
        tests[i].fn();
        printf("%-9s %s\n", tests[i].name, g_failures == before ? "OK" : "HATA");
    }
    network_free(&g_net);
    printf("\n%d kontrol, %d hata\n", g_checks, g_failures);
    return g_failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
