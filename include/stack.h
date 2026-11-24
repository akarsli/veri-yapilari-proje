#ifndef STACK_H
#define STACK_H

#include <stdbool.h>
#include <stddef.h>

/* Genel amaçlı (her türden eleman tutabilen) dinamik dizi tabanlı stack.
 *
 * Projede kullanım yerleri:
 *   - Rota geri izleme: hedeften kaynağa prev[] ile gidilir, stack'e itilir,
 *     sonra pop edilerek rota doğru sırada yazdırılır.
 *   - Undo / redo: yol kapatma/açma işlemleri iki stack'te tutulur.
 *
 * Karmaşıklık: push O(1) amortize, pop/peek O(1), bellek O(n). */
typedef struct {
    unsigned char *data;
    size_t elem_size;
    size_t size;
    size_t cap;
} Stack;

void   stack_init(Stack *s, size_t elem_size);
void   stack_free(Stack *s);
void   stack_clear(Stack *s);
void   stack_push(Stack *s, const void *elem);
bool   stack_pop(Stack *s, void *out);         /* out NULL olabilir */
bool   stack_peek(const Stack *s, void *out);
bool   stack_empty(const Stack *s);
size_t stack_size(const Stack *s);

#endif
