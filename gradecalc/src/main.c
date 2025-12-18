#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "csv.h"
#include "grades.h"

/* -------------------- Config (program-level) -------------------- */

typedef struct {
    double target;        // e.g. 70.0
    double assume_other;  // e.g. 70.0
} Config;

/* -------------------- Small helper types -------------------- */

typedef struct {
    double earned_points;     // points earned so far toward module final (0..100)
    double known_weight;      // total marked weight (0..100)
    double remaining_weight;  // 100 - known_weight
} ModuleProgress;

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
        c.mark = -1.0; // unknown by default

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

/* marks.csv is optional: if it can't be opened, we treat as "no marks yet" */
static int load_marks(ModuleList *modules, const char *path) {
    CsvFile *cf = csv_open(path);
    if (!cf) {
        // Optional file, not fatal.
        return 1;
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
        } else {
            // blank/invalid => leave unset
        }

        csv_row_free(&row);
    }

    csv_close(cf);
    return 1;
}

/* -------------------- Save marks.csv -------------------- */

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

/* -------------------- Progress calculations -------------------- */

static ModuleProgress module_progress(const Module *m) {
    ModuleProgress p = {0};

    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];
        if (c->mark >= 0.0) {
            p.earned_points += (c->mark * c->weight) / 100.0; // points toward final
            p.known_weight  += c->weight;
        }
    }

    p.remaining_weight = 100.0 - p.known_weight;
    return p;
}

/* -------------------- Reporting -------------------- */

static void print_module_stats(const Module *m, const Config *cfg) {
    const double TARGET = cfg->target;
    const double ASSUME_OTHER = cfg->assume_other;

    double S = 0.0; // sum(mark * weight)
    double W = 0.0; // sum(weight) for known marks

    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];
        if (c->mark >= 0.0) {
            S += c->mark * c->weight;
            W += c->weight;
        }
    }

    // Full module title as requested
    printf("%s (%d credits)\n", m->title, m->credits);

    if (W > 0.0) {
        printf("  Current average (marked work): %.2f%%\n", S / W);
    } else {
        printf("  Current average (marked work): (no marks yet)\n");
    }

    double R = 100.0 - W;
    if (R <= 0.0) {
        printf("  Final module mark: %.2f%%\n\n", S / 100.0);
        return;
    }

    double needed_avg = (TARGET * 100.0 - S) / R;
    printf("  Needed average on remaining to reach %.0f%%: %.2f%%\n",
           TARGET, needed_avg);

    printf("  Remaining assessments (assuming others get %.0f%%):\n", ASSUME_OTHER);

    int any_remaining = 0;
    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];
        if (c->mark >= 0.0) continue;

        any_remaining = 1;

        if (c->weight <= 0.0) {
            printf("    - %s (%.2f%%): cannot compute (weight is zero)\n",
                   c->name, c->weight);
            continue;
        }

        double other_weight = R - c->weight;
        // Need: S + (ASSUME_OTHER * other_weight) + (x * c->weight) >= TARGET*100
        double required =
            (TARGET * 100.0 - S - ASSUME_OTHER * other_weight) / c->weight;

        if (required > 100.0) {
            printf("    - %s (%.2f%%): need %.2f%% (impossible)\n",
                   c->name, c->weight, required);
        } else if (required < 0.0) {
            printf("    - %s (%.2f%%): need %.2f%% (already safe)\n",
                   c->name, c->weight, required);
        } else {
            printf("    - %s (%.2f%%): need %.2f%%\n",
                   c->name, c->weight, required);
        }
    }

    if (!any_remaining) {
        printf("    (none)\n");
    }

    printf("\n");
}

static void print_overall_summary(const ModuleList *modules, const Config *cfg) {
    const double target = cfg->target; // 0..100

    double total_credits = 0.0;

    // Achieved so far (credit-weighted points):
    // A = Σ credits * earned_points
    // Remaining influence:
    // B = Σ credits * (remaining_weight / 100)
    double A = 0.0;
    double B = 0.0;

    // For "current average on marked work"
    double known_credit_weight = 0.0; // Σ credits * (known_weight/100)
    double known_points = 0.0;        // Σ credits * (Σ(mark*weight)/100)

    for (size_t i = 0; i < modules->count; i++) {
        const Module *m = &modules->items[i];
        ModuleProgress p = module_progress(m);

        total_credits += m->credits;

        A += m->credits * p.earned_points;
        B += m->credits * (p.remaining_weight / 100.0);

        known_credit_weight += m->credits * (p.known_weight / 100.0);
        known_points += m->credits * p.earned_points;
    }

    printf("OVERALL (credit-weighted)\n");
    printf("  Total credits: %.0f\n", total_credits);

    if (known_credit_weight > 0.0) {
        double current_avg_marked = known_points / known_credit_weight;
        printf("  Current average on marked work: %.2f%%\n", current_avg_marked);
    } else {
        printf("  Current average on marked work: (no marks yet)\n");
    }

    if (B <= 0.0) {
        double final_overall = A / total_credits;
        printf("  No remaining assessments. Final overall: %.2f%%\n\n", final_overall);
        return;
    }

    double needed_remaining_avg = (target * total_credits - A) / B;

    printf("  Needed average on remaining work to reach %.0f%% overall: %.2f%%\n\n",
           target, needed_remaining_avg);
}

/* -------------------- Interactive input helpers -------------------- */

static void read_line(const char *prompt, char *buf, size_t buflen) {
    if (prompt && *prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    if (!fgets(buf, (int)buflen, stdin)) {
        buf[0] = '\0';
        return;
    }

    size_t n = strlen(buf);
    while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r')) {
        buf[--n] = '\0';
    }
}

static int read_int_prompt(const char *prompt, int *out) {
    char line[128];
    read_line(prompt, line, sizeof line);
    if (line[0] == '\0') return 0;

    char *end = NULL;
    long v = strtol(line, &end, 10);
    if (*end != '\0') return 0;

    *out = (int)v;
    return 1;
}

/* For marks: blank => clear (returns 2), number => set (returns 1), invalid => 0 */
static int read_optional_mark(const char *prompt, double *out_mark) {
    char line[128];
    read_line(prompt, line, sizeof line);

    if (line[0] == '\0') return 2; // clear

    char *end = NULL;
    double v = strtod(line, &end);
    if (*end != '\0') return 0;
    if (v < 0.0 || v > 100.0) return 0;

    *out_mark = v;
    return 1;
}

/* For config values: must enter a number 0..100 (no blank) */
static int read_double_in_range(const char *prompt, double minv, double maxv, double *out) {
    char line[128];
    read_line(prompt, line, sizeof line);
    if (line[0] == '\0') return 0;

    char *end = NULL;
    double v = strtod(line, &end);
    if (*end != '\0') return 0;
    if (v < minv || v > maxv) return 0;

    *out = v;
    return 1;
}

static Module *pick_module(ModuleList *modules) {
    printf("\nModules:\n");
    for (size_t i = 0; i < modules->count; i++) {
        Module *m = &modules->items[i];
        printf("  %d) %s\n", m->id, m->title);
    }

    int id = 0;
    if (!read_int_prompt("\nEnter module id (0 to cancel): ", &id)) return NULL;
    if (id == 0) return NULL;

    Module *m = module_list_find_by_id(modules, id);
    if (!m) printf("No module with id %d\n", id);
    return m;
}

static int pick_component_index(const Module *m) {
    printf("\nComponents for %s:\n", m->title);
    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];
        if (c->mark >= 0.0) {
            printf("  %zu) %s (%.2f%%)  mark=%.2f\n",
                   i + 1, c->name, c->weight, c->mark);
        } else {
            printf("  %zu) %s (%.2f%%)  mark=(unset)\n",
                   i + 1, c->name, c->weight);
        }
    }

    int idx = 0;
    if (!read_int_prompt("\nEnter component number (0 to cancel): ", &idx)) return -1;
    if (idx == 0) return -1;
    if (idx < 1 || (size_t)idx > m->component_count) {
        printf("Invalid component number.\n");
        return -1;
    }
    return idx - 1;
}

static void show_report(const ModuleList *modules, const Config *cfg) {
    printf("\n==== Report ====\n\n");
    printf("Target: %.2f%% | Assume other remaining: %.2f%%\n\n",
           cfg->target, cfg->assume_other);

    printf("Loaded %zu modules\n\n", modules->count);

    for (size_t i = 0; i < modules->count; i++) {
        print_module_stats(&modules->items[i], cfg);
    }

    print_overall_summary(modules, cfg);
}

/* -------------------- Interactive editor -------------------- */

static void edit_marks_menu(ModuleList *modules, Config *cfg) {
    while (1) {
        printf("\n==== Grade Tool ====\n");
        printf("1) Edit a mark\n");
        printf("2) Show report\n");
        printf("3) Save marks\n");
        printf("4) Set target overall (currently %.0f%%)\n", cfg->target);
        printf("5) Set assumed mark for other remaining (currently %.0f%%)\n", cfg->assume_other);
        printf("0) Exit\n");

        int choice = -1;
        if (!read_int_prompt("Choice: ", &choice)) {
            printf("Please enter a number.\n");
            continue;
        }

        if (choice == 0) return;

        if (choice == 1) {
            Module *m = pick_module(modules);
            if (!m) continue;

            int ci = pick_component_index(m);
            if (ci < 0) continue;

            Component *c = &m->components[ci];

            double new_mark = 0.0;
            int rc = read_optional_mark("Enter mark 0-100 (blank to clear): ", &new_mark);
            if (rc == 0) {
                printf("Invalid mark. Must be 0–100, or blank.\n");
            } else if (rc == 2) {
                c->mark = -1.0;
                printf("Cleared mark for '%s'.\n", c->name);
            } else {
                c->mark = new_mark;
                printf("Set '%s' to %.2f.\n", c->name, c->mark);
            }

        } else if (choice == 2) {
            show_report(modules, cfg);

        } else if (choice == 3) {
            if (save_marks_csv(modules, "data/marks.csv")) {
                printf("Saved data/marks.csv\n");
            } else {
                printf("Failed to save data/marks.csv\n");
            }

        } else if (choice == 4) {
            double v = 0.0;
            if (read_double_in_range("Enter target overall (0-100): ", 0.0, 100.0, &v)) {
                cfg->target = v;
                printf("Target set to %.2f%%\n", cfg->target);
            } else {
                printf("Invalid. Enter a number 0–100.\n");
            }

        } else if (choice == 5) {
            double v = 0.0;
            if (read_double_in_range("Enter assumed mark for others (0-100): ", 0.0, 100.0, &v)) {
                cfg->assume_other = v;
                printf("Assumption set to %.2f%%\n", cfg->assume_other);
            } else {
                printf("Invalid. Enter a number 0–100.\n");
            }

        } else {
            printf("Unknown choice.\n");
        }
    }
}

/* -------------------- main -------------------- */

int main(void) {
    ModuleList modules;
    module_list_init(&modules);

    Config cfg = { .target = 70.0, .assume_other = 70.0 };

    if (!load_modules(&modules, "data/modules.csv")) {
        module_list_free(&modules);
        return 1;
    }
    if (!load_components(&modules, "data/components.csv")) {
        module_list_free(&modules);
        return 1;
    }
    if (!load_marks(&modules, "data/marks.csv")) {
        module_list_free(&modules);
        return 1;
    }

    edit_marks_menu(&modules, &cfg);

    // Auto-save on exit (convenient)
    if (!save_marks_csv(&modules, "data/marks.csv")) {
        fprintf(stderr, "Warning: could not save data/marks.csv\n");
    }

    module_list_free(&modules);
    return 0;
}
