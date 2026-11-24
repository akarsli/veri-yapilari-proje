#ifndef ROUTE_H
#define ROUTE_H

#include <stdbool.h>
#include <stdio.h>

#include "graph.h"

/* Bulunan rota: (durak, hat) durumlarının sırası ve özet bilgiler. */
typedef struct {
    bool   found;
    int   *states;        /* başlangıçtan hedefe durum indeksleri */
    int    state_count;
    double total_min;     /* = ride + wait + walk                  */
    double ride_min;      /* araç içinde geçen süre                */
    double wait_min;      /* ilk biniş + aktarmalardaki bekleme    */
    double walk_min;      /* aktarma yürüyüşleri                   */
    double total_km;
    int    transfers;
    double avg_crowd;     /* uğranan durakların ortalama yoğunluğu (1-5) */
    double crowd_load;    /* kalabalık yükü: girilen her istasyon için (yoğunluk - 1) toplamı;
                             çok kriterli maliyetteki yoğunluk teriminin ağırlıksız hali */
    double cost;          /* aramanın en küçüklediği maliyet (ağırlıklı)   */
    int    expanded;      /* heap'ten çıkarılan durum sayısı (A* karşılaştırması için) */
} Route;

/* Çok kriterli rota ağırlıkları. Hepsi "dakika karşılığı" birimindedir:
 *   time     : her dakika yolculuk/bekleme/yürüme için çarpan (genelde 1)
 *   transfer : her aktarma için eklenen ceza (dk)
 *   crowd    : girilen her istasyon için (yoğunluk - 1) * crowd ceza
 *              (yoğunluk 1 = boş -> ceza yok, 5 = çok kalabalık -> 4 * crowd)
 * Dijkstra negatif kenar kabul etmediği için ağırlıklar >= 0 olmalı. */
typedef struct {
    double time;
    double transfer;
    double crowd;
} RouteWeights;

extern const RouteWeights WEIGHTS_FASTEST;    /* {1, 0, 0}: sadece süre       */
extern const RouteWeights WEIGHTS_BALANCED;   /* {1, 5, 0.5}                   */
extern const RouteWeights WEIGHTS_AVOID_CROWD;/* {1, 2, 3}: kalabalıktan kaçın */

/* En kısa süreli rota: Dijkstra + indeksli min-heap.
 *
 * Maliyet modeli:
 *   - ilk biniş            : hattın sefer aralığı / 2 (ortalama bekleme)
 *   - RIDE kenarı          : kenarın dakikası
 *   - TRANSFER kenarı      : yürüme + yeni hattın sefer aralığı / 2
 *   - kapalı kenarlar atlanır
 * Hedef durağın herhangi bir durumuna ilk ulaşıldığında arama durur.
 *
 * Karmaşıklık: O((V + E) log V) zaman, O(V) bellek (V = durum sayısı). */
bool route_fastest(const Network *net, int src_stop, int dst_stop, Route *out);

/* Çok kriterli rota: aynı Dijkstra, kenar maliyeti ağırlıklarla hesaplanır.
 *   RIDE     : time * dakika + crowd * (hedef istasyonun yoğunluğu - 1)
 *   TRANSFER : time * (yürüme + bekleme) + transfer
 * route_fastest(), WEIGHTS_FASTEST ile yapılan çağrının kendisidir.
 * Ağırlıklardan biri negatifse false döner. */
bool route_weighted(const Network *net, int src_stop, int dst_stop,
                    const RouteWeights *w, Route *out);

/* En kısa süreli rota, A* ile. Sonuç route_fastest() ile aynı sürededir;
 * fark, aramanın hedefe doğru yönlendirilmesi ve daha az durum açmasıdır.
 *
 * Sezgisel: h(n) = kuş uçuşu mesafe(n, hedef) / ağdaki en yüksek hız.
 * Hiçbir rota hedefe bu hızdan daha hızlı ulaşamayacağı için h gerçek
 * kalan süreyi asla aşmaz (kabul edilebilir / admissible); her kenar için
 * h(u) <= maliyet(u,v) + h(v) de sağlandığından tutarlıdır (consistent).
 * Bu yüzden kapalı küme (done[]) ile güvenle kullanılabilir.
 *
 * Karmaşıklık: en kötü durumda Dijkstra ile aynı O((V + E) log V);
 * pratikte açılan durum sayısı belirgin şekilde azalır. */
bool route_astar(const Network *net, int src_stop, int dst_stop, Route *out);

/* En az aktarmalı rota (iki aşama):
 *
 * 1) 0-1 BFS (çift uçlu kuyruk) ile en az aktarma sayısı K bulunur.
 *    RIDE kenarının maliyeti 0, TRANSFER kenarınınki 1'dir. Maliyeti 0 olan
 *    komşu kuyruğun ÖNÜNE, 1 olan ARKASINA eklenir; böylece kuyruk her an
 *    aktarma sayısına göre sıralı kalır (BFS'in katman katman ilerlemesi).
 *    O(V + E).
 * 2) En fazla K aktarmalı rotalar arasından en hızlısı seçilir: graf K+1
 *    katmana kopyalanır (düğüm = durum, kullanılan aktarma) ve Dijkstra
 *    çalıştırılır. O(K (V + E) log(K V)).
 *
 * İkinci aşama olmasa aynı aktarma sayısındaki rotalardan ilk bulunanı
 * dönerdi (örn. Kartal -> Emin Ali Paşa için 20 dk yürümeli Bostancı
 * aktarması, oysa Kozyatağı aktarması 9 dk daha hızlı). */
bool route_min_transfers(const Network *net, int src_stop, int dst_stop, Route *out);

void route_free(Route *r);

/* Rotayı hat hat (biniş, aktarma, iniş) okunabilir biçimde yazdırır. */
void route_print(const Network *net, const Route *r, FILE *fp);

#endif
