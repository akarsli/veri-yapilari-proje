#include "trie.h"

#include <stdlib.h>

#include "common.h"

/* Karakteri çocuk indeksine çevirir: ' ' -> 0, '0'-'9' -> 1-10, 'a'-'z' -> 11-36.
 * Bu sıra alfabetik sıralı sonuç vermeyi sağlar ("kartal" < "kartal 2"). */
static int char_index(char c)
{
    if (c == ' ') return 0;
    if (c >= '0' && c <= '9') return 1 + (c - '0');
    if (c >= 'a' && c <= 'z') return 11 + (c - 'a');
    return -1;
}

static TrieNode *node_new(Trie *t)
{
    t->node_count++;
    return xcalloc(1, sizeof(TrieNode));
}

static void node_free(TrieNode *n)
{
    if (!n) return;
    for (int i = 0; i < TRIE_ALPHABET; i++) node_free(n->child[i]);
    free(n->ids);
    free(n);
}

static const TrieNode *walk(const Trie *t, const char *key)
{
    const TrieNode *n = t->root;
    for (const char *p = key; *p && n; p++) {
        int ci = char_index(*p);
        if (ci < 0) return NULL;
        n = n->child[ci];
    }
    return n;
}

void trie_init(Trie *t)
{
    t->node_count = 0;
    t->root = node_new(t);
}

void trie_free(Trie *t)
{
    node_free(t->root);
    t->root = NULL;
    t->node_count = 0;
}

bool trie_insert(Trie *t, const char *key, int id)
{
    for (const char *p = key; *p; p++)
        if (char_index(*p) < 0) return false;

    TrieNode *n = t->root;
    for (const char *p = key; *p; p++) {
        int ci = char_index(*p);
        if (!n->child[ci]) n->child[ci] = node_new(t);
        n = n->child[ci];
    }
    if (n->id_count == n->id_cap) {
        n->id_cap = n->id_cap ? n->id_cap * 2 : 2;
        n->ids = xrealloc(n->ids, sizeof(int) * (size_t)n->id_cap);
    }
    n->ids[n->id_count++] = id;
    return true;
}

int trie_find_exact(const Trie *t, const char *key, int *out, int max)
{
    const TrieNode *n = walk(t, key);
    if (!n) return 0;
    for (int i = 0; i < n->id_count && i < max; i++) out[i] = n->ids[i];
    return n->id_count;
}

/* Alt ağacı önce-kök (pre-order) gezerek sonuçları toplar. */
static void collect(const TrieNode *n, int *out, int max, int *count)
{
    for (int i = 0; i < n->id_count && *count < max; i++) out[(*count)++] = n->ids[i];
    for (int c = 0; c < TRIE_ALPHABET && *count < max; c++)
        if (n->child[c]) collect(n->child[c], out, max, count);
}

int trie_prefix(const Trie *t, const char *prefix, int *out, int max)
{
    const TrieNode *n = walk(t, prefix);
    int count = 0;
    if (n && max > 0) collect(n, out, max, &count);
    return count;
}
