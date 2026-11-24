#include "io_csv.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define LINE_BUF   1024
#define MAX_FIELDS 16

/* ------------------------------------------------------------------ */
/* Yardımcılar                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    FILE       *fp;
    const char *name;   /* hata mesajları için dosya adı */
    int         line_no;
    char        buf[LINE_BUF];
} CsvReader;

static bool csv_open(CsvReader *r, const char *dir, const char *file, FILE *log)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, file);
    r->fp = fopen(path, "r");
    r->name = file;
    r->line_no = 0;
    if (!r->fp) {
        fprintf(log, "Hata: %s acilamadi (%s)\n", path, strerror(errno));
        return false;
    }
    return true;
}

/* Sonraki veri satırını okuyup alanlara böler. Başlık ve boş satırlar atlanır.
 * Alan sayısını döndürür; dosya bittiyse 0. Satır çok uzunsa -1. */
static int csv_next(CsvReader *r, char **fields)
{
    while (fgets(r->buf, sizeof r->buf, r->fp)) {
        r->line_no++;
        size_t len = strlen(r->buf);
        if (len == sizeof r->buf - 1 && r->buf[len - 1] != '\n') {
            int c;   /* satırın geri kalanını at */
            while ((c = fgetc(r->fp)) != '\n' && c != EOF) {}
            return -1;
        }
        while (len > 0 && (r->buf[len - 1] == '\n' || r->buf[len - 1] == '\r')) r->buf[--len] = '\0';
        if (r->line_no == 1 || len == 0) continue;   /* başlık / boş satır */

        /* Boş alanları da koruyarak virgülden böl (strtok boş alanı atlar) */
        int n = 0;
        char *p = r->buf;
        fields[n++] = p;
        for (; *p; p++) {
            if (*p == ',') {
                *p = '\0';
                if (n == MAX_FIELDS) return MAX_FIELDS + 1;
                fields[n++] = p + 1;
            }
        }
        return n;
    }
    return 0;
}

static void csv_warn(const CsvReader *r, FILE *log, LoadReport *rep, const char *msg, const char *detail)
{
    fprintf(log, "  %s:%d: %s%s%s\n", r->name, r->line_no, msg, detail ? " " : "", detail ? detail : "");
    rep->warnings++;
}

static bool parse_int(const char *s, int *out)
{
    char *end;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno) return false;
    *out = (int)v;
    return true;
}

static bool parse_double(const char *s, double *out)
{
    char *end;
    errno = 0;
    double v = strtod(s, &end);
    if (end == s || *end != '\0' || errno) return false;
    *out = v;
    return true;
}

/* Uzunluk kontrolüyle kopyalar; sığmazsa false. */
static bool copy_field(char *dst, size_t size, const char *src)
{
    size_t n = strlen(src);
    if (n >= size) return false;
    memcpy(dst, src, n + 1);
    return true;
}

/* ------------------------------------------------------------------ */
/* Dosya yükleyicileri                                                */
/* ------------------------------------------------------------------ */

/* id,name,lat,lon,crowd,connections */
static bool load_stops(Network *net, const char *dir, FILE *log, LoadReport *rep)
{
    CsvReader r;
    char *f[MAX_FIELDS];
    if (!csv_open(&r, dir, "stops.csv", log)) return false;

    int n;
    while ((n = csv_next(&r, f)) != 0) {
        if (n != 5 && n != 6) { csv_warn(&r, log, rep, "beklenen 5-6 alan, satir atlandi", NULL); continue; }

        Stop s;
        memset(&s, 0, sizeof s);
        if (!parse_int(f[0], &s.id) || s.id <= 0)       { csv_warn(&r, log, rep, "gecersiz id:", f[0]); continue; }
        if (f[1][0] == '\0' || !copy_field(s.name, sizeof s.name, f[1]))
                                                         { csv_warn(&r, log, rep, "gecersiz/cok uzun ad:", f[1]); continue; }
        if (!parse_double(f[2], &s.lat) || !parse_double(f[3], &s.lon))
                                                         { csv_warn(&r, log, rep, "gecersiz koordinat", NULL); continue; }
        if (!parse_int(f[4], &s.crowd) || s.crowd < 1 || s.crowd > 5) {
            csv_warn(&r, log, rep, "yogunluk 1-5 olmali, 3 kabul edildi:", f[4]);
            s.crowd = 3;
        }
        s.crowd_base = s.crowd;
        if (n == 6 && !copy_field(s.connections, sizeof s.connections, f[5]))
            csv_warn(&r, log, rep, "baglanti bilgisi cok uzun, kesildi", NULL);
        network_append_stop(net, &s);
    }
    fclose(r.fp);

    if (net->stop_count == 0) {
        fprintf(log, "Hata: stops.csv icinde gecerli durak yok\n");
        return false;
    }
    return network_index_stops(net, log);
}

/* line_id,name,type,color,headway_min,total_min,air_km */
static bool load_lines(Network *net, const char *dir, FILE *log, LoadReport *rep)
{
    CsvReader r;
    char *f[MAX_FIELDS];
    if (!csv_open(&r, dir, "lines.csv", log)) return false;

    int n;
    while ((n = csv_next(&r, f)) != 0) {
        if (n != 7) { csv_warn(&r, log, rep, "beklenen 7 alan, satir atlandi", NULL); continue; }

        Line l;
        memset(&l, 0, sizeof l);
        if (f[0][0] == '\0' || !copy_field(l.id, sizeof l.id, f[0])) { csv_warn(&r, log, rep, "gecersiz hat kodu:", f[0]); continue; }
        if (!copy_field(l.name, sizeof l.name, f[1]) || !copy_field(l.type, sizeof l.type, f[2]) ||
            !copy_field(l.color, sizeof l.color, f[3]))           { csv_warn(&r, log, rep, "alan cok uzun", NULL); continue; }
        if (!parse_double(f[4], &l.headway_min) || l.headway_min <= 0)
                                                                     { csv_warn(&r, log, rep, "gecersiz sefer araligi:", f[4]); continue; }
        if (!parse_double(f[5], &l.total_min)) l.total_min = 0;
        if (network_append_line(net, &l) < 0) csv_warn(&r, log, rep, "hat kodu tekrarlaniyor:", f[0]);
    }
    fclose(r.fp);

    if (net->line_count == 0) {
        fprintf(log, "Hata: lines.csv icinde gecerli hat yok\n");
        return false;
    }
    return true;
}

/* from_id,to_id,line_id,minutes,distance_km */
static bool load_edges(Network *net, const char *dir, FILE *log, LoadReport *rep)
{
    CsvReader r;
    char *f[MAX_FIELDS];
    if (!csv_open(&r, dir, "edges.csv", log)) return false;

    int n;
    while ((n = csv_next(&r, f)) != 0) {
        if (n != 5) { csv_warn(&r, log, rep, "beklenen 5 alan, satir atlandi", NULL); continue; }

        int from_id, to_id;
        double minutes, km;
        if (!parse_int(f[0], &from_id) || !parse_int(f[1], &to_id)) { csv_warn(&r, log, rep, "gecersiz durak id", NULL); continue; }
        int a = network_find_stop_by_id(net, from_id);
        int b = network_find_stop_by_id(net, to_id);
        if (a < 0 || b < 0) { csv_warn(&r, log, rep, "bilinmeyen durak id", a < 0 ? f[0] : f[1]); continue; }
        if (a == b)         { csv_warn(&r, log, rep, "durak kendine bagli", NULL); continue; }
        int line = network_find_line(net, f[2]);
        if (line < 0)       { csv_warn(&r, log, rep, "bilinmeyen hat:", f[2]); continue; }
        if (!parse_double(f[3], &minutes) || minutes <= 0) { csv_warn(&r, log, rep, "gecersiz sure:", f[3]); continue; }
        if (!parse_double(f[4], &km) || km < 0)            { csv_warn(&r, log, rep, "gecersiz mesafe:", f[4]); continue; }
        if (!network_add_ride(net, a, b, line, minutes, km))
            csv_warn(&r, log, rep, "durakta cok fazla hat var", NULL);
    }
    fclose(r.fp);
    return true;
}

/* stop_id,from_line,to_line,walk_min */
static bool load_transfers(Network *net, const char *dir, FILE *log, LoadReport *rep)
{
    CsvReader r;
    char *f[MAX_FIELDS];
    if (!csv_open(&r, dir, "transfers.csv", log)) return false;

    int n;
    while ((n = csv_next(&r, f)) != 0) {
        if (n != 4) { csv_warn(&r, log, rep, "beklenen 4 alan, satir atlandi", NULL); continue; }

        int stop_id;
        double walk;
        if (!parse_int(f[0], &stop_id)) { csv_warn(&r, log, rep, "gecersiz durak id:", f[0]); continue; }
        int stop = network_find_stop_by_id(net, stop_id);
        if (stop < 0) { csv_warn(&r, log, rep, "bilinmeyen durak id:", f[0]); continue; }
        int from = network_find_line(net, f[1]), to = network_find_line(net, f[2]);
        if (from < 0 || to < 0) { csv_warn(&r, log, rep, "bilinmeyen hat", from < 0 ? f[1] : f[2]); continue; }
        if (!parse_double(f[3], &walk) || walk < 0) { csv_warn(&r, log, rep, "gecersiz yurume suresi:", f[3]); continue; }
        if (!network_add_transfer(net, stop, from, to, walk))
            csv_warn(&r, log, rep, "durak bu iki hatta birden bulunmuyor", net->stops[stop].name);
    }
    fclose(r.fp);
    return true;
}

bool network_load(Network *net, const char *dir, FILE *log, LoadReport *report)
{
    LoadReport local = {0};
    if (!report) report = &local;
    report->warnings = 0;

    return load_stops(net, dir, log, report) &&
           load_lines(net, dir, log, report) &&
           load_edges(net, dir, log, report) &&
           load_transfers(net, dir, log, report);
}
