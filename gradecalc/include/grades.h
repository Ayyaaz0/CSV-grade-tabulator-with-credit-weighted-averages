#ifndef GRADES_H
#define GRADES_H

#include <stddef.h>

typedef struct {
    char name[128];
    double weight;   // percent (e.g. 25.0)
    double mark;     // 0–100, or -1.0 if unknown
} Component;

typedef struct {
    int id;
    char code[32];
    char title[128];
    int credits;

    Component *components;
    size_t component_count;
    size_t component_capacity;
} Module;

typedef struct {
    Module *items;
    size_t count;
    size_t capacity;
} ModuleList;

void module_list_init(ModuleList *list);
void module_list_free(ModuleList *list);
int  module_list_push(ModuleList *list, const Module *m);

Module *module_list_find_by_id(ModuleList *list, int id);
Component *module_find_component_by_name(Module *m, const char *name);
int module_add_component(Module *m, const Component *c);

#endif
