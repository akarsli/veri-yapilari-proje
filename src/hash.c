#include "hash.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

/* FNV-1a 64 bit: basit, hızlı ve string'lerde iyi dağılım verir. */
static uint64_t fnv1a(const char *s)
{
    uint64_t h = 14695981039346656037ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h ^= *p;
        h *= 1099511628211ULL;
    }
    return h;
}

static size_t bucket_of(const HashMap *m, const char *key)
{
    return (size_t)(fnv1a(key) % m->bucket_count);
}

static void rehash(HashMap *m, size_t new_count)
{
    HashEntry **new_buckets = xcalloc(new_count, sizeof(HashEntry *));
    for (size_t i = 0; i < m->bucket_count; i++) {
        HashEntry *e = m->buckets[i];
        while (e) {
            HashEntry *next = e->next;
            size_t b = (size_t)(fnv1a(e->key) % new_count);
            e->next = new_buckets[b];
            new_buckets[b] = e;
            e = next;
        }
    }
    free(m->buckets);
    m->buckets = new_buckets;
    m->bucket_count = new_count;
}

void hash_init(HashMap *m, size_t initial_buckets)
{
    m->bucket_count = initial_buckets ? initial_buckets : 16;
    m->buckets = xcalloc(m->bucket_count, sizeof(HashEntry *));
    m->size = 0;
}

void hash_free(HashMap *m)
{
    for (size_t i = 0; i < m->bucket_count; i++) {
        HashEntry *e = m->buckets[i];
        while (e) {
            HashEntry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    m->buckets = NULL;
    m->bucket_count = m->size = 0;
}

void hash_put(HashMap *m, const char *key, int value)
{
    size_t b = bucket_of(m, key);
    for (HashEntry *e = m->buckets[b]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            e->value = value;
            return;
        }
    }

    HashEntry *e = xmalloc(sizeof *e);
    e->key = xstrdup(key);
    e->value = value;
    e->next = m->buckets[b];   /* zincirin başına ekle: O(1) */
    m->buckets[b] = e;
    m->size++;

    if (m->size * 4 > m->bucket_count * 3)   /* doluluk > 0.75 */
        rehash(m, m->bucket_count * 2);
}

bool hash_get(const HashMap *m, const char *key, int *value)
{
    for (HashEntry *e = m->buckets[bucket_of(m, key)]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (value) *value = e->value;
            return true;
        }
    }
    return false;
}

bool hash_remove(HashMap *m, const char *key)
{
    HashEntry **link = &m->buckets[bucket_of(m, key)];
    while (*link) {
        HashEntry *e = *link;
        if (strcmp(e->key, key) == 0) {
            *link = e->next;
            free(e->key);
            free(e);
            m->size--;
            return true;
        }
        link = &e->next;
    }
    return false;
}

size_t hash_size(const HashMap *m)
{
    return m->size;
}
