#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "csv.h"
#include "grades.h"


static int parse_int(const char *s, int *out) {
    if (!s || !*s) return 0;
    char *end;
    long v = strtol(s, &end, 10);
    if (*end != '\0') return 0;
    *out = (int)v;
    return 1;
}

static int parse_double(const char *s, double *out) {
    if (!s || !*s) return 0;
    char *end;
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
        if (rc < 0) { csv_close(cf); return 0; }

        if (first) { first = 0; csv_row_free(&row); continue; }
        if (row.count < 4) { csv_row_free(&row); continue; }

        Module m = {0};
        if (!parse_int(row.fields[0], &m.id) ||
            !parse_int(row.fields[3], &m.credits)) {
            csv_row_free(&row);
            continue;
        }

        snprintf(m.code, sizeof m.code, "%s", row.fields[1]);
        snprintf(m.title, sizeof m.title, "%s", row.fields[2]);

        if (!module_list_push(modules, &m)) {
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
    if (!cf) return 0;

    CsvRow row;
    int first = 1;

    while (1) {
        int rc = csv_read_row(cf, &row);
        if (rc == 0) break;
        if (rc < 0) { csv_close(cf); return 0; }

        if (first) { first = 0; csv_row_free(&row); continue; }
        if (row.count < 3) { csv_row_free(&row); continue; }

        int module_id;
        double weight;
        if (!parse_int(row.fields[0], &module_id) ||
            !parse_double(row.fields[2], &weight)) {
            csv_row_free(&row);
            continue;
        }

        Module *m = module_list_find_by_id(modules, module_id);
        if (!m) { csv_row_free(&row); continue; }

        Component c = {0};
        snprintf(c.name, sizeof c.name, "%s", row.fields[1]);
        c.weight = weight;
        c.mark = -1.0;  // unknown by default

        module_add_component(m, &c);
        csv_row_free(&row);
    }

    csv_close(cf);
    return 1;
}

static int load_marks(ModuleList *modules, const char *path) {
    CsvFile *cf = csv_open(path);
    if (!cf) return 0;

    CsvRow row;
    int first = 1;

    while (1) {
        int rc = csv_read_row(cf, &row);
        if (rc == 0) break;
        if (rc < 0) { csv_close(cf); return 0; }

        if (first) { first = 0; csv_row_free(&row); continue; }
        if (row.count < 3) { csv_row_free(&row); continue; }

        int module_id;
        double mark;
        if (!parse_int(row.fields[0], &module_id)) {
            csv_row_free(&row);
            continue;
        }

        Module *m = module_list_find_by_id(modules, module_id);
        if (!m) { csv_row_free(&row); continue; }

        Component *c = module_find_component_by_name(m, row.fields[1]);
        if (!c) { csv_row_free(&row); continue; }

        if (parse_double(row.fields[2], &mark))
            c->mark = mark;

        csv_row_free(&row);
    }

    csv_close(cf);
    return 1;
}

static void print_module_stats(const Module *m) {
    double S = 0.0, W = 0.0;

    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];
        if (c->mark >= 0.0) {
            S += c->mark * c->weight;
            W += c->weight;
        }
    }

    printf("%s (%d credits)\n", m->code, m->credits);

    if (W > 0.0) {
        printf("  Current average: %.2f%%\n", S / W);
    } else {
        printf("  No marks yet\n");
    }

    double R = 100.0 - W;
    if (R > 0.0) {
        printf("  Needed on remaining for 70%%: %.2f%%\n",
               (70.0 * 100.0 - S) / R);
    } else {
        printf("  Final mark: %.2f%%\n", S / 100.0);
    }

    printf("\n");
}

int main(void) {
    ModuleList modules;
    module_list_init(&modules);

    load_modules(&modules, "data/modules.csv");
    load_components(&modules, "data/components.csv");
    load_marks(&modules, "data/marks.csv");

    printf("Loaded %zu modules\n\n", modules.count);

    for (size_t i = 0; i < modules.count; i++) {
        print_module_stats(&modules.items[i]);
    }

    module_list_free(&modules);
    return 0;
}
