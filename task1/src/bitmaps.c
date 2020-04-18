#include <stdio.h>

#include "exit_codes.h"

int bitmap_set(FILE *file, size_t offset, size_t idx, char value) {
    fseek(file, offset + idx / 8, SEEK_SET);
    char byte;
    char new_byte;
    if (fread(&byte, 1, 1, file) != 1) return READ_FAILURE;
    if (value == 1) {
        new_byte = byte | (1 << (idx % 8));
    } else {
        new_byte = byte & (~(1 << (idx % 8)));
    }
    fseek(file, -1, SEEK_CUR);
    if (fwrite(&new_byte, 1, 1, file) != 1) return WRITE_FAILURE;
    return 0;
}

int bitmap_read(FILE *file, size_t offset, size_t idx) {
    fseek(file, offset + idx / 8, SEEK_SET);
    char byte;
    if (fread(&byte, 1, 1, file) != 1) return READ_FAILURE;
    return (byte >> (idx % 8)) & 1;
}

int bitmap_find_first_free(FILE *file, size_t offset, size_t bitmap_length, size_t *results, size_t required) {
    size_t byte_idx = 0;
    size_t found = 0;
    fseek(file, offset, SEEK_SET);
    char byte;
    while (byte_idx < bitmap_length && found < required) {
        if (fread(&byte, 1, 1, file) != 1) return READ_FAILURE;
        size_t bit_idx = 0;
        while (bit_idx < 8 && found < required) {
            if (((byte >> bit_idx) & 1) == 0) {
                results[found] = byte_idx * 8 + bit_idx;
                found++;
            }
            bit_idx++;
        }
        byte_idx++;
    }
    if (found < required) return NO_SPACE;
    return 0;
}