#ifndef IO_H
#define IO_H

#include "grades.h"

int load_modules(ModuleList *modules, const char *path);
int load_components(ModuleList *modules, const char *path);
int load_marks(ModuleList *modules, const char *path);
int save_marks_csv(const ModuleList *modules, const char *path);

#endif
