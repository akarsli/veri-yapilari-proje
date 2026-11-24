#ifndef HISTORY_H
#define HISTORY_H

#include <stdbool.h>

#include "graph.h"
#include "stack.h"

/* Kapalı yol işlemleri için geri al / yinele (undo / redo).
 *
 * İki stack kullanılır:
 *   - undo: yapılan işlemler (en sonuncusu tepede)
 *   - redo: geri alınan işlemler
 * Yeni bir işlem yapıldığında redo stack'i temizlenir (metin editörlerindeki
 * davranışla aynı). Her işlem O(1) stack operasyonu + O(derece) kenar
 * güncellemesidir. */
typedef struct {
    int  stop_a, stop_b, line;
    bool closed;   /* işlemden SONRAKİ durum: true = kapatıldı, false = açıldı */
} ClosureOp;

typedef struct {
    Stack undo;
    Stack redo;
} History;

void history_init(History *h);
void history_free(History *h);

typedef enum {
    APPLY_OK,
    APPLY_NO_SEGMENT,     /* bu hatta bu iki istasyon komşu değil */
    APPLY_UNCHANGED       /* zaten istenen durumda */
} ApplyResult;

/* Bağlantıyı kapatır veya açar ve işlemi geçmişe kaydeder. */
ApplyResult history_apply(History *h, Network *net, int stop_a, int stop_b, int line, bool close);

/* Son işlemi geri alır / yeniden yapar. Yapılacak işlem yoksa false.
 * done NULL değilse etkilenen işlem buraya yazılır. */
bool history_undo(History *h, Network *net, ClosureOp *done);
bool history_redo(History *h, Network *net, ClosureOp *done);

#endif
