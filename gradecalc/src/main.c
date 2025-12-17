#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "csv.h"
#include "grades.h"

static int is_header(const CsvRow *row, const char *col0) {
    return row->count > 0 && strcmp(row->fields[0], col0) == 0;
}

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

static int load_modules(ModuleList *modules, const char *path) {
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
        if (rc < 0) { fprintf(stderr, "CSV read error in %s\n", path); csv_close(cf); return 0; }

        if (first && is_header(&row, "module_id")) {
            first = 0;
            csv_row_free(&row);
            continue;
        }
        first = 0;

        if (row.count < 4) { csv_row_free(&row); continue; }

        Module m = {0};
        if (!parse_int(row.fields[0], &m.id) || !parse_int(row.fields[3], &m.credits)) {
            csv_row_free(&row);
            continue;
        }

        snprintf(m.code, sizeof m.code, "%s", row.fields[1]);
        snprintf(m.title, sizeof m.title, "%s", row.fields[2]);

        if (!module_list_push(modules, &m)) {
            fprintf(stderr, "Out of memory while adding module\n");
            csv_row_free(&row);
            csv_close(cf);
            return 0;
        }

        csv_row_free(&row);
    }

    csv_close(cf);
    return 1;
}

static int load_components(ModuleList *modules, const char *path) {
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
        if (rc < 0) { fprintf(stderr, "CSV read error in %s\n", path); csv_close(cf); return 0; }

        if (first && is_header(&row, "module_id")) {
            first = 0;
            csv_row_free(&row);
            continue;
        }
        first = 0;

        if (row.count < 3) { csv_row_free(&row); continue; }

        int module_id = 0;
        double weight = 0.0;
        if (!parse_int(row.fields[0], &module_id) || !parse_double(row.fields[2], &weight)) {
            csv_row_free(&row);
            continue;
        }

        Module *m = module_list_find_by_id(modules, module_id);
        if (!m) {
            fprintf(stderr, "Warning: component refers to unknown module_id %d\n", module_id);
            csv_row_free(&row);
            continue;
        }

        Component c = {0};
        snprintf(c.name, sizeof c.name, "%s", row.fields[1]);
        c.weight = weight;

        if (!module_add_component(m, &c)) {
            fprintf(stderr, "Out of memory while adding component\n");
            csv_row_free(&row);
            csv_close(cf);
            return 0;
        }

        csv_row_free(&row);
    }

    csv_close(cf);
    return 1;
}

static void validate_weights(const ModuleList *modules) {
    const double EPS = 1e-6;

    for (size_t i = 0; i < modules->count; i++) {
        const Module *m = &modules->items[i];
        double sum = 0.0;
        for (size_t j = 0; j < m->component_count; j++) {
            sum += m->components[j].weight;
        }

        if (m->component_count == 0) {
            printf("WARNING: %s has no components\n", m->code);
        } else if (fabs(sum - 100.0) > EPS) {
            printf("WARNING: %s weights sum to %.2f (expected 100.00)\n", m->code, sum);
        }
    }
}

static void print_modules(const ModuleList *modules) {
    printf("Loaded %zu modules:\n\n", modules->count);

    for (size_t i = 0; i < modules->count; i++) {
        const Module *m = &modules->items[i];
        printf("[%d] %s — %s (%d credits)\n", m->id, m->code, m->title, m->credits);

        for (size_t j = 0; j < m->component_count; j++) {
            const Component *c = &m->components[j];
            printf("   - %s: %.2f%%\n", c->name, c->weight);
        }
        printf("\n");
    }
}

int main(void) {
    ModuleList modules;
    module_list_init(&modules);

    if (!load_modules(&modules, "data/modules.csv")) {
        module_list_free(&modules);
        return 1;
    }

    if (!load_components(&modules, "data/components.csv")) {
        module_list_free(&modules);
        return 1;
    }

    validate_weights(&modules);
    print_modules(&modules);

    module_list_free(&modules);
    return 0;
}
