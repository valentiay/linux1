#include <stdlib.h>
#include <string.h>
#include "files.h"
#include "exit_codes.h"

int dir_init(struct FS *fs) {
    char content = 'e';
    return file_add(fs, &content, 1, 1);
}

int dir_add(struct FS *fs, char *filename, size_t file_inode_idx, size_t dir_inode_idx) {
    size_t filename_size = strlen(filename) + 1;

    int buffer_size = file_size(fs, dir_inode_idx) + sizeof(size_t) + filename_size;
    if (buffer_size < 0) return buffer_size;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    memcpy(buffer + buffer_size, &file_inode_idx, sizeof(size_t));
    memcpy(buffer + buffer_size + sizeof(size_t), filename, filename_size);

    res = file_update(fs, dir_inode_idx, buffer, buffer_size + sizeof(size_t) + filename_size, 1);
    free(buffer);
    if (res < 0) return res;

    return 0;
}

int dir_remove_rec(struct FS *fs, size_t dir_inode_idx) {
    int buffer_size = file_size(fs, dir_inode_idx);
    if (buffer_size < 0) return 0;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t i = 1;
    while (i < buffer_size) {
        size_t file_inode_idx;
        memcpy(&file_inode_idx, buffer + i, sizeof(size_t));
        i += sizeof(size_t);
        res = file_remove(fs, file_inode_idx, 0);
        if (res == WRONG_FILE_TYPE) {
            dir_remove_rec(fs, file_inode_idx);
        }
        if (res < 0) {
            free(buffer);
            return res;
        }

        size_t filename_size = strlen(buffer + i) + 1;
        i += filename_size;
    }
    res = file_remove(fs, dir_inode_idx, 1);
    free(buffer);
    if (res < 0) return res;
    return 0;
}

int dir_remove_file(struct FS *fs, char *filename, size_t dir_inode_idx) {
    int buffer_size = file_size(fs, dir_inode_idx);
    if(buffer_size < 0) return buffer_size;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t i = 1;
    int found = 0;
    size_t file_inode_idx = 0;
    size_t filename_size = 0;

    while (i < buffer_size && found == 0) {
        memcpy(&file_inode_idx, buffer + i, sizeof(size_t));
        i += sizeof(size_t);
        if (strcmp(filename, buffer + i) == 0) {
            found = 1;
        }
        filename_size = strlen(buffer + i) + 1;
        i += filename_size;
    }

    if (found == 0) {
        free(buffer);
        return NOT_FOUND;
    }

    res = file_remove(fs, file_inode_idx, 0);
    if (res == WRONG_FILE_TYPE) {
        res = dir_remove_rec(fs, file_inode_idx);
    }
    if (res < 0) {
        free(buffer);
        return res;
    }

    memmove(buffer + i - sizeof(size_t) - filename_size, buffer + i, buffer_size - i);
    res = file_update(fs, dir_inode_idx, buffer, buffer_size - sizeof(size_t) - filename_size, 1);
    free(buffer);
    if (res < 0) return res;

    return 0;
}

int dir_list(struct FS *fs, size_t dir_inode_idx) {
    int buffer_size = file_size(fs, dir_inode_idx);
    if (buffer_size < 0) return buffer_size;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t i = 1;
    while (i < buffer_size) {
        size_t file_inode_idx;
        memcpy(&file_inode_idx, buffer + i, sizeof(size_t));
        i += sizeof(size_t);
        printf("%lu: %s\n", file_inode_idx, buffer + i);
        size_t filename_size = strlen(buffer + i) + 1;
        i += filename_size;
    }
    free(buffer);
    return 0;
}

int dir_find(struct FS *fs, char *filename, size_t dir_inode_idx) {
    int buffer_size = file_size(fs, dir_inode_idx);
    if (buffer_size < 0) return buffer_size;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t i = 1;
    while (i < buffer_size) {
        size_t file_inode_idx;
        memcpy(&file_inode_idx, buffer + i, sizeof(size_t));
        i += sizeof(size_t);
        if (strcmp(filename, buffer + i) == 0) {
            free(buffer);
            return file_inode_idx;
        }
        size_t filename_size = strlen(buffer + i) + 1;
        i += filename_size;
    }
    free(buffer);
    return NOT_FOUND;
}