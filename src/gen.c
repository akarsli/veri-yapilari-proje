#include "gen.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define SPACING_KM   0.8
#define JITTER_KM    0.15
#define SPEED_KM_MIN 0.6    /* ~36 km/s ortalama ticari hız */
#define DWELL_MIN    0.5

/* Tekrarlanabilir rastgele sayı üreteci (LCG) */
static unsigned g_rng;

static unsigned rnd(void)
{
    g_rng = g_rng * 1103515245u + 12345u;
    return g_rng >> 16;
}

static double rnd_unit(void)   /* [-1, 1] */
{
    return (double)(rnd() % 20001u) / 10000.0 - 1.0;
}

bool gen_grid_city(Network *net, int size, unsigned seed)
{
    if (size < 2 || size > 2000) return false;
    g_rng = seed ? seed : 1u;

    const double lat0 = 41.0, lon0 = 29.0;
    const double km_per_lat = 111.0;
    const double km_per_lon = 111.0 * cos(lat0 * 3.14159265358979323846 / 180.0);

    /* Duraklar: id = r * size + c + 1 */
    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            Stop s;
            memset(&s, 0, sizeof s);
            s.id = r * size + c + 1;
            snprintf(s.name, sizeof s.name, "D%d", s.id);
            s.lat = lat0 + (r * SPACING_KM + JITTER_KM * rnd_unit()) / km_per_lat;
            s.lon = lon0 + (c * SPACING_KM + JITTER_KM * rnd_unit()) / km_per_lon;
            s.crowd = s.crowd_base = 1 + (int)(rnd() % 5u);
            network_append_stop(net, &s);
        }
    }
    if (!network_index_stops(net, stderr)) return false;

    /* Hatlar: önce satırlar (R), sonra sütunlar (C) */
    for (int k = 0; k < 2 * size; k++) {
        Line l;
        memset(&l, 0, sizeof l);
        snprintf(l.id, sizeof l.id, "%c%d", k < size ? 'R' : 'C', k % size);
        snprintf(l.type, sizeof l.type, "BUS");
        l.headway_min = 3 + (double)(rnd() % 8u);
        if (network_append_line(net, &l) < 0) return false;
    }

    /* Bağlantılar: durak indeksleri id sırasındadır (index_stops sıraladı) */
    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            int here = r * size + c;
            if (c + 1 < size) {   /* satır hattı: sağ komşu */
                double km = network_stop_distance_km(net, here, here + 1);
                network_add_ride(net, here, here + 1, r, km / SPEED_KM_MIN + DWELL_MIN, km);
            }
            if (r + 1 < size) {   /* sütun hattı: alt komşu */
                double km = network_stop_distance_km(net, here, here + size);
                network_add_ride(net, here, here + size, size + c, km / SPEED_KM_MIN + DWELL_MIN, km);
            }
        }
    }

    /* Aktarmalar: her durakta satır <-> sütun hattı */
    for (int r = 0; r < size; r++) {
        for (int c = 0; c < size; c++) {
            int here = r * size + c;
            double walk = 1 + (double)(rnd() % 4u);
            network_add_transfer(net, here, r, size + c, walk);
            network_add_transfer(net, here, size + c, r, walk);
        }
    }
    return true;
}
