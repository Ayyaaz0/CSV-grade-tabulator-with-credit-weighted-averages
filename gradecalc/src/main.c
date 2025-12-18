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
    const double TARGET = 70.0;
    const double ASSUME_OTHER = 70.0; // assumption for "other remaining components"

    double S = 0.0, W = 0.0; // S = Σ(mark * weight), W = Σ(weight) for known marks

    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];
        if (c->mark >= 0.0) {
            S += c->mark * c->weight;
            W += c->weight;
        }
    }

    printf("%s (%d credits)\n", m->title, m->credits);

    if (W > 0.0) {
        printf("  Current average (marked work): %.2f%%\n", S / W);
    } else {
        printf("  Current average (marked work): (no marks yet)\n");
    }

    double R = 100.0 - W; // remaining weight
    if (R > 0.0) {
        double needed_avg = (TARGET * 100.0 - S) / R;
        printf("  Needed average on remaining to reach %.0f%%: %.2f%%\n", TARGET, needed_avg);
    } else {
        printf("  Final module mark: %.2f%%\n", S / 100.0);
        printf("\n");
        return;
    }

    // Per-component requirement (given assumption about other remaining components)
    printf("  Remaining assessments (needed if others score %.0f%%):\n", ASSUME_OTHER);

    int any_remaining = 0;
    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];
        if (c->mark >= 0.0) continue; // already known

        any_remaining = 1;

        double other_weight = R - c->weight;
        if (c->weight <= 0.0) {
            printf("    - %s (%.2f%%): cannot compute (weight is zero)\n", c->name, c->weight);
            continue;
        }

        // Need: S + (ASSUME_OTHER * other_weight) + (x * c->weight) >= TARGET*100
        double required = (TARGET * 100.0 - S - ASSUME_OTHER * other_weight) / c->weight;

        // Display nicely with feasibility info
        if (required > 100.0) {
            printf("    - %s (%.2f%%): need %.2f%% (impossible > 100%%)\n",
                   c->name, c->weight, required);
        } else if (required < 0.0) {
            printf("    - %s (%.2f%%): need %.2f%% (already safe; 0%% would still do)\n",
                   c->name, c->weight, required);
        } else {
            printf("    - %s (%.2f%%): need %.2f%%\n", c->name, c->weight, required);
        }
    }

    if (!any_remaining) {
        printf("    (none)\n");
    }

    printf("\n");
}

typedef struct {
    double earned_points;     // points earned so far toward module final (0..100)
    double known_weight;      // total marked weight (0..100)
    double remaining_weight;  // 100 - known_weight
} ModuleProgress;

static ModuleProgress module_progress(const Module *m) {
    ModuleProgress p = {0};

    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];
        if (c->mark >= 0.0) {
            p.earned_points += (c->mark * c->weight) / 100.0; // contributes to final mark
            p.known_weight  += c->weight;
        }
    }

    p.remaining_weight = 100.0 - p.known_weight;
    return p;
}

static void print_overall_summary(const ModuleList *modules, double target_overall) {
    double total_credits = 0.0;

    // Credit-weighted "points" bookkeeping:
    // A = achieved credit-points so far (credits * earned_points)
    // B = remaining credit-weight still to be graded (credits * remaining_weight)
    double A = 0.0;
    double B = 0.0;

    for (size_t i = 0; i < modules->count; i++) {
        const Module *m = &modules->items[i];
        ModuleProgress p = module_progress(m);

        total_credits += m->credits;
        A += m->credits * p.earned_points;        // earned_points is already out of 100
        B += m->credits * p.remaining_weight;     // remaining_weight is out of 100
    }

    printf("OVERALL (credit-weighted)\n");
    printf("  Total credits: %.0f\n", total_credits);

    // current overall % on completed work (credit-weighted), if any marks exist:
    double known_credit_weight = 0.0;
    for (size_t i = 0; i < modules->count; i++) {
        const Module *m = &modules->items[i];
        ModuleProgress p = module_progress(m);
        known_credit_weight += m->credits * p.known_weight;
    }

    if (known_credit_weight > 0.0) {
        double current_completed_avg = (A / known_credit_weight) * 100.0;
        printf("  Current average on marked work: %.2f%%\n", current_completed_avg);
    } else {
        printf("  Current average on marked work: (no marks yet)\n");
    }

    // Required average on remaining work to hit target_overall:
    // Need total credit-points T = target_overall * total_credits * 100
    // because module marks are /100 but A is credits * points(out of 100).
    double T = target_overall * total_credits * 100.0;

    if (B <= 0.0) {
        // Nothing left to influence: final overall is A / (total_credits*100) * 100
        double final_overall = (A / (total_credits * 100.0)) * 100.0;
        printf("  No remaining assessments. Final overall: %.2f%%\n", final_overall);
        return;
    }

    double needed_remaining_avg = ((T - A) / B) * 100.0;
    printf("  Needed average on remaining work to reach %.0f%% overall: %.2f%%\n",
           target_overall * 100.0, needed_remaining_avg);
    printf("\n");
}

static int save_marks_csv(const ModuleList *modules, const char *path) {
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

            // If component names might contain commas, you’d quote them.
            // For your current names, commas are unlikely, so we keep it simple.
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

    load_marks(&modules, "data/marks.csv");

    printf("Loaded %zu modules\n\n", modules.count);

    for (size_t i = 0; i < modules.count; i++) {
        print_module_stats(&modules.items[i]);
    }

    print_overall_summary(&modules, 0.70);

    if (!save_marks_csv(&modules, "data/marks.csv")) {
        fprintf(stderr, "Warning: could not save data/marks.csv\n");
        module_list_free(&modules);
        return 1;
    }

    module_list_free(&modules);
    return 0;
}
