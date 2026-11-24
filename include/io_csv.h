#ifndef IO_CSV_H
#define IO_CSV_H

#include <stdbool.h>
#include <stdio.h>

#include "graph.h"

typedef struct {
    int warnings;        /* atlanan hatalı satır sayısı */
} LoadReport;

/* dir klasöründeki stops.csv, lines.csv, edges.csv ve transfers.csv
 * dosyalarından ağı kurar. net önceden network_init() ile hazırlanmış olmalı.
 *
 * Hatalı satırlar "dosya:satır: açıklama" biçiminde log'a yazılır ve atlanır.
 * Dosya açılamazsa veya hiç durak/hat yoksa false döner. */
bool network_load(Network *net, const char *dir, FILE *log, LoadReport *report);

#endif
