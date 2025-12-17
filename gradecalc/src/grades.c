#include "grades.h"
#include <stdlib.h>
#include <string.h>

void module_list_init(ModuleList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void module_init(Module *m) {
    m->components = NULL;
    m->component_count = 0;
    m->component_capacity = 0;
}

static void module_free(Module *m) {
    free(m->components);
    m->components = NULL;
    m->component_count = 0;
    m->component_capacity = 0;
}

void module_list_free(ModuleList *list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; i++) {
        module_free(&list->items[i]);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

int module_list_push(ModuleList *list, const Module *m) {
    if (list->count == list->capacity) {
        size_t newcap = (list->capacity == 0) ? 8 : list->capacity * 2;
        Module *newitems = (Module *)realloc(list->items, newcap * sizeof(Module));
        if (!newitems) return 0;
        list->items = newitems;
        list->capacity = newcap;
    }

    list->items[list->count] = *m;       // struct copy
    module_init(&list->items[list->count]); // ensure components start empty
    list->count++;
    return 1;
}

Module *module_list_find_by_id(ModuleList *list, int id) {
    if (!list) return NULL;
    for (size_t i = 0; i < list->count; i++) {
        if (list->items[i].id == id) return &list->items[i];
    }
    return NULL;
}

int module_add_component(Module *m, const Component *c) {
    if (!m || !c) return 0;

    if (m->component_count == m->component_capacity) {
        size_t newcap = (m->component_capacity == 0) ? 4 : m->component_capacity * 2;
        Component *newitems = (Component *)realloc(m->components, newcap * sizeof(Component));
        if (!newitems) return 0;
        m->components = newitems;
        m->component_capacity = newcap;
    }

    m->components[m->component_count++] = *c; // struct copy
    return 1;
}

