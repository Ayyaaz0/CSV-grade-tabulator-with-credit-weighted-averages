#include <stdio.h>

#include "grades.h"
#include "config.h"
#include "io.h"
#include "ui.h"

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

    ui_run(&modules, &cfg);

    // Auto-save on exit
    if (!save_marks_csv(&modules, "data/marks.csv")) {
        fprintf(stderr, "Warning: could not save data/marks.csv\n");
    }

    module_list_free(&modules);
    return 0;
}
