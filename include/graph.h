#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "hash.h"
#include "trie.h"

/* Toplu taşıma ağı.
 *
 * MODEL: Graf düğümleri durak değil, (durak, hat) çiftleridir; bunlara
 * "durum" (state) diyoruz. Örneğin Kozyatağı iki durumdan oluşur:
 * (Kozyatağı, M4) ve (Kozyatağı, M8).
 *   - RIDE kenarı    : aynı hatta komşu iki istasyon arası yolculuk
 *   - TRANSFER kenarı: aynı istasyonda hat değiştirme (yürüme + bekleme)
 * Böylece algoritmalar hangi hatta olunduğunu bilir ve aktarmaları sayabilir.
 *
 * Kenarlar her durumun kendi dinamik dizisinde tutulur (adjacency list).
 * Bellek O(V + E). */

#define NAME_MAX_LEN       64
#define MAX_LINES_PER_STOP 8

typedef struct {
    int    id;                       /* CSV'deki id                          */
    char   name[NAME_MAX_LEN];       /* görünen ad (UTF-8)                   */
    char   key[NAME_MAX_LEN];        /* normalleştirilmiş arama anahtarı     */
    double lat, lon;
    int    crowd;                    /* güncel (canlı) yoğunluk puanı (1-5)  */
    int    crowd_base;               /* CSV'deki temel yoğunluk              */
    char   connections[96];          /* kapsam dışı bağlantılar (bilgi)      */
    int    states[MAX_LINES_PER_STOP];
    int    state_count;
} Stop;

typedef struct {
    char   id[8];                    /* "M4"                                 */
    char   name[96];
    char   type[16];
    char   color[8];
    double headway_min;              /* sefer aralığı                        */
    double total_min;                /* uçtan uca resmi süre                 */
} Line;

typedef enum { EDGE_RIDE, EDGE_TRANSFER } EdgeKind;

typedef struct {
    int      to;                     /* hedef durum indeksi                  */
    double   minutes;                /* RIDE: yolculuk, TRANSFER: yürüme     */
    double   km;
    EdgeKind kind;
    bool     closed;                 /* kapalı yol (dinamik kaldırma)        */
} Edge;

typedef struct {
    int   stop;
    int   line;
    Edge *edges;
    int   edge_count, edge_cap;
} State;

typedef struct {
    Stop   *stops;  int stop_count,  stop_cap;
    Line   *lines;  int line_count,  line_cap;
    State  *states; int state_count, state_cap;
    int     segment_count;           /* yönsüz RIDE bağlantı sayısı          */
    double  max_speed;               /* en hızlı bağlantının kuş uçuşu hızı
                                        (km/dk); A* sezgiseli için           */
    HashMap stop_by_key;             /* normalleştirilmiş ad -> durak        */
    HashMap line_by_id;              /* "M4" -> hat                          */
    Trie    names;                   /* ad önek araması                      */
} Network;

void network_init(Network *net);
void network_free(Network *net);

/* --- Kurulum (CSV yükleyici kullanır) --- */
int  network_append_stop(Network *net, const Stop *s);
int  network_append_line(Network *net, const Line *l);      /* id tekrarsa -1 */
/* Durakları id'ye göre sıralar (quicksort), hash ve trie dizinlerini kurar.
 * Tekrarlanan id varsa false. */
bool network_index_stops(Network *net, FILE *log);
bool network_add_ride(Network *net, int stop_a, int stop_b, int line,
                      double minutes, double km);
bool network_add_transfer(Network *net, int stop, int from_line, int to_line,
                          double walk_min);

/* --- Coğrafya --- */
/* İki koordinat arası kuş uçuşu mesafe (haversine formülü), km. */
double geo_distance_km(double lat1, double lon1, double lat2, double lon2);
double network_stop_distance_km(const Network *net, int stop_a, int stop_b);

/* --- Sorgular --- */
int  network_find_stop_by_id(const Network *net, int csv_id);   /* binary search */
int  network_find_line(const Network *net, const char *line_id); /* hash */
int  network_state(const Network *net, int stop, int line);      /* yoksa -1 */

/* Ad araması: önce tam eşleşme (hash, O(1)), yoksa önek (trie).
 * Bulunan durak indekslerini out'a yazar, sayısını döndürür. */
int  network_search_stops(const Network *net, const char *query, int *out, int max);

/* Durağın hatlarını "M4, M8" biçiminde yazar. */
void network_stop_lines(const Network *net, int stop, char *buf, size_t size);

/* Bir hattaki iki komşu istasyon arasındaki bağlantıyı (iki yönde) kapatır
 * veya açar. Bağlantı yoksa false. */
bool network_set_segment_closed(Network *net, int stop_a, int stop_b, int line, bool closed);

/* --- Canlı yoğunluk --- */
#define CROWD_MIN 1
#define CROWD_MAX 5

/* Durağın güncel yoğunluğunu ayarlar (1-5 aralığına sıkıştırılır). */
void network_set_crowd(Network *net, int stop, int crowd);
/* Tüm durakları CSV'deki temel değerlere döndürür. */
void network_reset_crowd(Network *net);
/* Canlı veri simülasyonu: her durak için temel değer + [-1, +2] rastgele
 * sapma (yoğun saat etkisi). Aynı seed aynı sonucu verir. */
void network_simulate_crowd(Network *net, unsigned seed);

/* Bağlantının durumu: -1 böyle bir bağlantı yok, 0 açık, 1 kapalı. */
int  network_segment_status(const Network *net, int stop_a, int stop_b, int line);

#endif
