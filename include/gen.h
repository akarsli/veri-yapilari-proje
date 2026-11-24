#ifndef GEN_H
#define GEN_H

#include <stdbool.h>

#include "graph.h"

/* Performans testleri için sentetik "ızgara şehir" üreteci.
 *
 * size x size duraklık bir ızgara kurar (duraklar arası ~0.8 km, küçük
 * rastgele kayma). Her satır bir hat (R0, R1, ...), her sütun bir hat
 * (C0, C1, ...) olur; böylece her durak iki hattın kesişimidir ve iki
 * yönlü aktarma içerir. Sefer aralıkları (3-10 dk) ve yürüme süreleri
 * (1-4 dk) rastgeledir.
 *
 * Boyutlar: durak = size^2, durum = 2 * size^2, hat = 2 * size.
 * Aynı seed her zaman aynı ağı üretir. net önceden network_init() ile
 * hazırlanmış olmalı. */
bool gen_grid_city(Network *net, int size, unsigned seed);

#endif
