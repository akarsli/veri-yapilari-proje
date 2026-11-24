#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void out_of_memory(size_t size)
{
    fprintf(stderr, "Hata: bellek ayrilamadi (%zu bayt)\n", size);
    exit(EXIT_FAILURE);
}

void *xmalloc(size_t size)
{
    void *p = malloc(size ? size : 1);
    if (!p) out_of_memory(size);
    return p;
}

void *xcalloc(size_t count, size_t size)
{
    void *p = calloc(count ? count : 1, size ? size : 1);
    if (!p) out_of_memory(count * size);
    return p;
}

void *xrealloc(void *ptr, size_t size)
{
    void *p = realloc(ptr, size ? size : 1);
    if (!p) out_of_memory(size);
    return p;
}

char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *copy = xmalloc(n);
    memcpy(copy, s, n);
    return copy;
}
