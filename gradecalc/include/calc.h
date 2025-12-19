#ifndef CALC_H
#define CALC_H

#include "grades.h"

// Computes weighted sums with optional best-of grouping.
// S = Σ(mark * weight) over counted items
// W = Σ(weight) over marked items that count
// R = Σ(weight) over remaining items that count
void module_sums_bestof(const Module *m, double *outS, double *outW, double *outR);

#endif
