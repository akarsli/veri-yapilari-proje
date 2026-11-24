#ifndef TEXT_H
#define TEXT_H

#include <stddef.h>

/* Durak adını arama anahtarına çevirir:
 *   - Türkçe harfler ASCII karşılığına indirgenir (ç->c, ğ->g, ı/İ->i, ö->o, ş->s, ü->u)
 *   - Harfler küçültülür
 *   - Harf/rakam dışındaki her şey ('-', '.', boşluk dizileri) tek boşluk olur
 *   - Baştaki ve sondaki boşluklar silinir
 * Örnek: "Yakacık-Adnan Kahveci" -> "yakacik adnan kahveci"
 *        "KADIKÖY"               -> "kadikoy"
 * Böylece kullanıcı Türkçe klavyesi olmadan da arama yapabilir.
 * out en fazla out_size-1 karakter alır, her zaman '\0' ile biter. */
void text_normalize(const char *in, char *out, size_t out_size);

#endif
