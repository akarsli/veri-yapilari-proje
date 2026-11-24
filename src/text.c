#include "text.h"

#include <ctype.h>

/* İki baytlık UTF-8 Türkçe karakterler ve ASCII karşılıkları */
static const struct {
    unsigned char b1, b2;
    char ascii;
} TURKISH[] = {
    {0xC3, 0xA7, 'c'}, {0xC3, 0x87, 'c'},   /* ç Ç */
    {0xC4, 0x9F, 'g'}, {0xC4, 0x9E, 'g'},   /* ğ Ğ */
    {0xC4, 0xB1, 'i'}, {0xC4, 0xB0, 'i'},   /* ı İ */
    {0xC3, 0xB6, 'o'}, {0xC3, 0x96, 'o'},   /* ö Ö */
    {0xC5, 0x9F, 's'}, {0xC5, 0x9E, 's'},   /* ş Ş */
    {0xC3, 0xBC, 'u'}, {0xC3, 0x9C, 'u'},   /* ü Ü */
    {0xC3, 0xA2, 'a'}, {0xC3, 0xAE, 'i'},   /* â î */
    {0xC3, 0xBB, 'u'},                      /* û   */
};

/* Bir UTF-8 karakteri okur, ASCII karşılığını *ch'ye yazar (karşılığı
 * yoksa 0) ve kaç bayt tüketildiğini döndürür. */
static size_t read_char(const unsigned char *p, char *ch)
{
    if (*p < 0x80) {
        *ch = (char)*p;
        return 1;
    }
    for (size_t i = 0; i < sizeof TURKISH / sizeof TURKISH[0]; i++) {
        if (p[0] == TURKISH[i].b1 && p[1] == TURKISH[i].b2) {
            *ch = TURKISH[i].ascii;
            return 2;
        }
    }
    /* Tanınmayan çok baytlı karakter: devam baytlarını atla */
    size_t n = 1;
    while ((p[n] & 0xC0) == 0x80) n++;
    *ch = 0;
    return n;
}

void text_normalize(const char *in, char *out, size_t out_size)
{
    if (out_size == 0) return;

    const unsigned char *p = (const unsigned char *)in;
    size_t len = 0;
    int pending_space = 0;

    while (*p) {
        char ch;
        p += read_char(p, &ch);

        if (ch && isalnum((unsigned char)ch)) {
            if (pending_space && len > 0 && len + 1 < out_size) out[len++] = ' ';
            pending_space = 0;
            if (len + 1 < out_size) out[len++] = (char)tolower((unsigned char)ch);
        } else {
            pending_space = 1;   /* ayraçlar tek boşluğa iner */
        }
    }
    out[len] = '\0';
}
