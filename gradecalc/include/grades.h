#ifndef GRADES_H
#define GRADES_H

#include <stddef.h>

typedef struct {
    char name[64];
    double weight;
    double mark;   // -1 if unknown

    // Best-of-N grouping
    int group_id;  // 0 = not grouped; >0 = belongs to a group
    int best_of;   // 0 = normal; >0 = count only best N in the group
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
