#ifndef CSV_H
#define CSV_H

#include <stddef.h>

typedef struct {
    char **fields;
    size_t count;
} CsvRow;

typedef struct CsvFile CsvFile;

CsvFile *csv_open(const char *path);
void     csv_close(CsvFile *f);

// Reads next row. Returns 1 if row read, 0 on EOF, -1 on error.
int      csv_read_row(CsvFile *f, CsvRow *out);

// Free memory owned by a row
void     csv_row_free(CsvRow *row);

#endif
