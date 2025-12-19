#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "csv.h"
#include "grades.h"
#include "io.h"

/* -------------------- Parsing helpers -------------------- */

static int parse_int(const char *s, int *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (*end != '\0') return 0;
    *out = (int)v;
    return 1;
}

static int parse_double(const char *s, double *out) {
    if (!s || !*s) return 0;
    char *end = NULL;
    double v = strtod(s, &end);
    if (*end != '\0') return 0;
    *out = v;
    return 1;
}

/* -------------------- CSV loaders -------------------- */

int load_modules(ModuleList *modules, const char *path) {
    CsvFile *cf = csv_open(path);
    if (!cf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 0;
    }

    CsvRow row;
    int first = 1;

    while (1) {
        int rc = csv_read_row(cf, &row);
        if (rc == 0) break;
        if (rc < 0) {
            fprintf(stderr, "CSV read error in %s\n", path);
            csv_close(cf);
            return 0;
        }

        if (first) { first = 0; csv_row_free(&row); continue; }
        if (row.count < 4) { csv_row_free(&row); continue; }

        Module m = (Module){0};
        if (!parse_int(row.fields[0], &m.id) ||
            !parse_int(row.fields[3], &m.credits)) {
            csv_row_free(&row);
            continue;
        }

        snprintf(m.code, sizeof m.code, "%s", row.fields[1]);
        snprintf(m.title, sizeof m.title, "%s", row.fields[2]);

        if (!module_list_push(modules, &m)) {
            fprintf(stderr, "Out of memory adding module\n");
            csv_row_free(&row);
            csv_close(cf);
            return 0;
        }

        csv_row_free(&row);
    }

    csv_close(cf);
    return 1;
}

/*
components.csv supported formats:

OLD:
  module_id,component_name,weight

NEW (optional, for best-of-N grouping):
  module_id,component_name,weight,group_id,best_of
*/
int load_components(ModuleList *modules, const char *path) {
    CsvFile *cf = csv_open(path);
    if (!cf) {
        fprintf(stderr, "Failed to open %s\n", path);
        return 0;
    }

    CsvRow row;
    int first = 1;

    while (1) {
        int rc = csv_read_row(cf, &row);
        if (rc == 0) break;
        if (rc < 0) {
            fprintf(stderr, "CSV read error in %s\n", path);
            csv_close(cf);
            return 0;
        }

        if (first) { first = 0; csv_row_free(&row); continue; }
        if (row.count < 3) { csv_row_free(&row); continue; }

        int module_id = 0;
        double weight = 0.0;

        if (!parse_int(row.fields[0], &module_id) ||
            !parse_double(row.fields[2], &weight)) {
            csv_row_free(&row);
            continue;
        }

        Module *m = module_list_find_by_id(modules, module_id);
        if (!m) {
            fprintf(stderr, "Warning: component refers to unknown module_id %d\n", module_id);
            csv_row_free(&row);
            continue;
        }

        Component c = (Component){0};
        snprintf(c.name, sizeof c.name, "%s", row.fields[1]);
        c.weight = weight;
        c.mark = -1.0;

        // Best-of-N defaults (requires Component to have these fields)
        c.group_id = 0;
        c.best_of  = 0;

        if (row.count >= 5) {
            (void)parse_int(row.fields[3], &c.group_id);
            (void)parse_int(row.fields[4], &c.best_of);
        }

        if (!module_add_component(m, &c)) {
            fprintf(stderr, "Out of memory adding component\n");
            csv_row_free(&row);
            csv_close(cf);
            return 0;
        }

        csv_row_free(&row);
    }

    csv_close(cf);
    return 1;
}

/* marks.csv is optional */
int load_marks(ModuleList *modules, const char *path) {
    CsvFile *cf = csv_open(path);
    if (!cf) return 1;

    CsvRow row;
    int first = 1;

    while (1) {
        int rc = csv_read_row(cf, &row);
        if (rc == 0) break;
        if (rc < 0) {
            fprintf(stderr, "CSV read error in %s\n", path);
            csv_close(cf);
            return 0;
        }

        if (first) { first = 0; csv_row_free(&row); continue; }
        if (row.count < 3) { csv_row_free(&row); continue; }

        int module_id = 0;
        if (!parse_int(row.fields[0], &module_id)) {
            csv_row_free(&row);
            continue;
        }

        Module *m = module_list_find_by_id(modules, module_id);
        if (!m) { csv_row_free(&row); continue; }

        Component *c = module_find_component_by_name(m, row.fields[1]);
        if (!c) { csv_row_free(&row); continue; }

        double mark = 0.0;
        if (parse_double(row.fields[2], &mark)) {
            c->mark = mark;
        }

        csv_row_free(&row);
    }

    csv_close(cf);
    return 1;
}

/* -------------------- Save marks.csv -------------------- */

int save_marks_csv(const ModuleList *modules, const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "Failed to write %s\n", path);
        return 0;
    }

    fprintf(fp, "module_id,component_name,mark\n");

    for (size_t i = 0; i < modules->count; i++) {
        const Module *m = &modules->items[i];
        for (size_t j = 0; j < m->component_count; j++) {
            const Component *c = &m->components[j];
            if (c->mark >= 0.0) {
                fprintf(fp, "%d,%s,%.2f\n", m->id, c->name, c->mark);
            } else {
                fprintf(fp, "%d,%s,\n", m->id, c->name);
            }
        }
    }

    fclose(fp);
    return 1;
}
