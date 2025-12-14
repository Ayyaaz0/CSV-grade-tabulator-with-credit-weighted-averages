#ifndef GRADES_H
#define GRADES_H

#include <stddef.h>

typedef struct {
    int id;
    char code[32];
    char title[128];
    int credits;
} Module;

typedef struct {
    Module *items;
    size_t count;
    size_t capacity;
} ModuleList;

void module_list_init(ModuleList *list);
void module_list_free(ModuleList *list);
int  module_list_push(ModuleList *list, const Module *m);

#endif
