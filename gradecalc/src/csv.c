#include "csv.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct CsvFile {
    FILE *fp;
    char *line;
    size_t linecap;
};

static char *str_dup_range(const char *start, const char *end) {
    size_t n = (size_t)(end - start);
    char *s = (char *)malloc(n + 1);
    if (!s) return NULL;
    memcpy(s, start, n);
    s[n] = '\0';
    return s;
}

static void trim_in_place(char *s) {
    // trims whitespace around unquoted fields
    char *p = s;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p != s) memmove(s, p, strlen(p) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

CsvFile *csv_open(const char *path) {
    CsvFile *f = (CsvFile *)calloc(1, sizeof(CsvFile));
    if (!f) return NULL;

    f->fp = fopen(path, "r");
    if (!f->fp) {
        free(f);
        return NULL;
    }
    return f;
}

void csv_close(CsvFile *f) {
    if (!f) return;
    if (f->fp) fclose(f->fp);
    free(f->line);
    free(f);
}

void csv_row_free(CsvRow *row) {
    if (!row) return;
    for (size_t i = 0; i < row->count; i++) free(row->fields[i]);
    free(row->fields);
    row->fields = NULL;
    row->count = 0;
}

static int push_field(CsvRow *row, char *field) {
    char **nf = (char **)realloc(row->fields, (row->count + 1) * sizeof(char *));
    if (!nf) return 0;
    row->fields = nf;
    row->fields[row->count++] = field;
    return 1;
}

int csv_read_row(CsvFile *f, CsvRow *out) {
    if (!f || !out) return -1;
    out->fields = NULL;
    out->count = 0;

    ssize_t got = getline(&f->line, &f->linecap, f->fp);
    if (got < 0) return 0; // EOF

    // Strip newline(s)
    while (got > 0 && (f->line[got - 1] == '\n' || f->line[got - 1] == '\r')) {
        f->line[--got] = '\0';
    }

    const char *p = f->line;
    while (*p) {
        // Parse one field
        if (*p == '"') {
            // quoted field
            p++; // skip opening quote
            const char *start = p;
            char *buf = NULL;
            size_t buflen = 0;

            while (*p) {
                if (*p == '"' && p[1] == '"') {
                    // escaped quote ""
                    // append segment up to quote
                    size_t seg = (size_t)(p - start);
                    char *nb = (char *)realloc(buf, buflen + seg + 1);
                    if (!nb) { free(buf); csv_row_free(out); return -1; }
                    buf = nb;
                    memcpy(buf + buflen, start, seg);
                    buflen += seg;
                    buf[buflen] = '\0';

                    // append a literal quote
                    nb = (char *)realloc(buf, buflen + 2);
                    if (!nb) { free(buf); csv_row_free(out); return -1; }
                    buf = nb;
                    buf[buflen++] = '"';
                    buf[buflen] = '\0';

                    p += 2;
                    start = p;
                    continue;
                }
                if (*p == '"') break;
                p++;
            }

            // append tail
            size_t seg = (size_t)(p - start);
            char *nb = (char *)realloc(buf, buflen + seg + 1);
            if (!nb) { free(buf); csv_row_free(out); return -1; }
            buf = nb;
            memcpy(buf + buflen, start, seg);
            buflen += seg;
            buf[buflen] = '\0';

            if (*p == '"') p++; // closing quote

            // after quoted field, consume optional whitespace then optional comma
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p == ',') p++;

            if (!push_field(out, buf)) { free(buf); csv_row_free(out); return -1; }
        } else {
            // unquoted field: read until comma or end
            const char *start = p;
            while (*p && *p != ',') p++;
            const char *end = p;
            if (*p == ',') p++;

            char *field = str_dup_range(start, end);
            if (!field) { csv_row_free(out); return -1; }
            trim_in_place(field);
            if (!push_field(out, field)) { free(field); csv_row_free(out); return -1; }
        }
    }

    // Handle trailing comma -> empty last field (e.g. "a,b,")
    if (got > 0 && f->line[got - 1] == ',') {
        char *field = (char *)calloc(1, 1);
        if (!field) { csv_row_free(out); return -1; }
        if (!push_field(out, field)) { free(field); csv_row_free(out); return -1; }
    }

    return 1;
}
