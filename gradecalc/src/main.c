#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"
#include "grades.h"

static int is_header_row(const CsvRow *row) {
    return row->count >= 4 && strcmp(row->fields[0], "module_id") == 0;
}

static int parse_int(const char *s, int *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (*end != '\0') return 0;
    if (v < -2147483648L || v > 2147483647L) return 0;
    *out = (int)v;
    return 1;
}

int main(void) {
    const char *path = "data/modules.csv";

    CsvFile *cf = csv_open(path);
    if (!cf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 1;
    }

    ModuleList modules;
    module_list_init(&modules);

    CsvRow row;
    int first = 1;

    while (1) {
        int rc = csv_read_row(cf, &row);
        if (rc == 0) break;        // EOF
        if (rc < 0) {              // error
            fprintf(stderr, "CSV read error\n");
            csv_close(cf);
            module_list_free(&modules);
            return 1;
        }

        if (first && is_header_row(&row)) {
            first = 0;
            csv_row_free(&row);
            continue;
        }
        first = 0;

        if (row.count < 4) {
            fprintf(stderr, "Skipping invalid row (need 4 columns)\n");
            csv_row_free(&row);
            continue;
        }

        Module m = {0};

        if (!parse_int(row.fields[0], &m.id) ||
            !parse_int(row.fields[3], &m.credits)) {
            fprintf(stderr, "Skipping row: bad integers\n");
            csv_row_free(&row);
            continue;
        }

        snprintf(m.code, sizeof m.code, "%s", row.fields[1]);
        snprintf(m.title, sizeof m.title, "%s", row.fields[2]);

        if (!module_list_push(&modules, &m)) {
            fprintf(stderr, "Out of memory\n");
            csv_row_free(&row);
            csv_close(cf);
            module_list_free(&modules);
            return 1;
        }

        csv_row_free(&row);
    }

    csv_close(cf);

    printf("Loaded %zu modules:\n\n", modules.count);
    for (size_t i = 0; i < modules.count; i++) {
        Module *m = &modules.items[i];
        printf("[%d] %s — %s (%d credits)\n", m->id, m->code, m->title, m->credits);
    }

    module_list_free(&modules);
    return 0;
}
