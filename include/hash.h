#ifndef HASH_H
#define HASH_H

#include <stdbool.h>
#include <stddef.h>

/* Ayrı zincirleme (separate chaining) ile çakışma çözen hash tablosu:
 * string anahtar -> int değer.
 *
 * Projede: normalleştirilmiş durak adı -> durak indeksi, hat kodu -> hat
 * indeksi. Hash fonksiyonu FNV-1a. Doluluk oranı 0.75'i geçince kova sayısı
 * ikiye katlanır (rehash).
 *
 * Karmaşıklık: put/get/remove ortalama O(1), en kötü O(n); bellek O(n). */
typedef struct HashEntry {
    char *key;
    int value;
    struct HashEntry *next;
} HashEntry;

typedef struct {
    HashEntry **buckets;
    size_t bucket_count;
    size_t size;
} HashMap;

void   hash_init(HashMap *m, size_t initial_buckets);
void   hash_free(HashMap *m);
void   hash_put(HashMap *m, const char *key, int value);   /* varsa günceller */
bool   hash_get(const HashMap *m, const char *key, int *value);
bool   hash_remove(HashMap *m, const char *key);
size_t hash_size(const HashMap *m);

#endif
