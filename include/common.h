#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stddef.h>

/* Bellek ayırma sarmalayıcıları: başarısız olursa hata mesajı basıp programı
 * sonlandırır. Böylece her çağrıda NULL kontrolü yapmak gerekmez. */
void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s);

#endif
