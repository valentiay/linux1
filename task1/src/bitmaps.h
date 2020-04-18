#ifndef TASK1_BITMAPS_H
#define TASK1_BITMAPS_H

#include <stdio.h>

int bitmap_set(FILE *file, size_t offset, size_t idx, char value);

int bitmap_read(FILE *file, size_t offset, size_t idx);

int bitmap_find_first_free(FILE *file, size_t offset, size_t bitmap_length, size_t *results, size_t required);

#endif //TASK1_BITMAPS_H
