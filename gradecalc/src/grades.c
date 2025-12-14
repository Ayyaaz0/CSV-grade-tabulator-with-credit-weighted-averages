#include "grades.h"
#include <stdlib.h>
#include <string.h>

void module_list_init(ModuleList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void module_list_free(ModuleList *list) {
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
    list->items[list->count++] = *m; // struct copy
    return 1;
}
