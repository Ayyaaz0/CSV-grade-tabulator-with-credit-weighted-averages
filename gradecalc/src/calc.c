#include <stdlib.h>

#include "grades.h"
#include "calc.h"

/* -------------------- Best-of-N core (optional) -------------------- */

static int cmp_desc_double(const void *a, const void *b) {
    double da = *(const double*)a;
    double db = *(const double*)b;
    if (da < db) return 1;
    if (da > db) return -1;
    return 0;
}

void module_sums_bestof(const Module *m, double *outS, double *outW, double *outR) {
    double S = 0.0, W = 0.0, R = 0.0;

    for (size_t i = 0; i < m->component_count; i++) {
        const Component *c = &m->components[i];

        // Normal component
        if (c->group_id == 0 || c->best_of == 0) {
            if (c->mark >= 0.0) { S += c->mark * c->weight; W += c->weight; }
            else { R += c->weight; }
            continue;
        }

        // Grouped: process each group once
        int gid = c->group_id;
        int best_of = c->best_of;

        int already_done = 0;
        for (size_t k = 0; k < i; k++) {
            if (m->components[k].group_id == gid && m->components[k].best_of == best_of) {
                already_done = 1;
                break;
            }
        }
        if (already_done) continue;

        double marks[256];
        int nmarks = 0;
        double item_weight = -1.0;

        for (size_t j = 0; j < m->component_count; j++) {
            const Component *cj = &m->components[j];
            if (cj->group_id != gid || cj->best_of != best_of) continue;

            if (item_weight < 0.0) item_weight = cj->weight;

            if (cj->mark >= 0.0 && nmarks < (int)(sizeof marks / sizeof marks[0])) {
                marks[nmarks++] = cj->mark;
            }
        }

        if (item_weight < 0.0) continue;

        qsort(marks, (size_t)nmarks, sizeof(double), cmp_desc_double);

        int counted = (nmarks < best_of) ? nmarks : best_of;
        for (int t = 0; t < counted; t++) {
            S += marks[t] * item_weight;
            W += item_weight;
        }

        if (counted < best_of) {
            R += (best_of - counted) * item_weight;
        }
    }

    *outS = S;
    *outW = W;
    *outR = R;
}
