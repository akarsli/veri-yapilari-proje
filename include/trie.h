#ifndef TRIE_H
#define TRIE_H

#include <stdbool.h>
#include <stddef.h>

/* Önek (prefix) ağacı: durak adlarında otomatik tamamlama.
 * "kad" yazıldığında "kadikoy" ile başlayan tüm duraklar bulunur.
 *
 * Anahtarlar text_normalize() çıktısıdır; alfabe a-z, 0-9 ve boşluk
 * (37 sembol). Her düğüm, o noktada biten adların durak indekslerini tutar.
 * Sonuçlar alfabetik sırayla döner (çocuklar sırayla gezildiği için).
 *
 * Karmaşıklık (L = anahtar uzunluğu, k = bulunan sonuç, s = alt ağaç boyutu):
 *   insert O(L), exact O(L), prefix O(L + s); bellek O(toplam karakter * 37). */
#define TRIE_ALPHABET 37

typedef struct TrieNode {
    struct TrieNode *child[TRIE_ALPHABET];
    int *ids;      /* bu düğümde biten adların durak indeksleri */
    int id_count;
    int id_cap;
} TrieNode;

typedef struct {
    TrieNode *root;
    size_t node_count;
} Trie;

void trie_init(Trie *t);
void trie_free(Trie *t);

/* key alfabe dışı karakter içerirse false döner ve hiçbir şey eklenmez. */
bool trie_insert(Trie *t, const char *key, int id);

/* Tam eşleşen adların indekslerini out'a yazar (en fazla max). Bulunan
 * toplam sayıyı döndürür. */
int trie_find_exact(const Trie *t, const char *key, int *out, int max);

/* prefix ile başlayan adların indekslerini alfabetik sırayla out'a yazar
 * (en fazla max). out'a yazılan sayıyı döndürür. */
int trie_prefix(const Trie *t, const char *prefix, int *out, int max);

#endif
