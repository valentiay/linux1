#include <stdlib.h>
#include <string.h>
#include "files.h"
#include "exit_codes.h"

int dir_init(struct FS *fs) {
    size_t size = 0;
    return file_add(fs, (char *) &size, sizeof(size_t), 1);
}

int dir_add(struct FS *fs, char *filename, size_t file_inode_idx, size_t dir_inode_idx) {
    size_t filename_size = strlen(filename) + 1;

    size_t dir_size = file_size(fs, dir_inode_idx, 1);
    if (dir_size < 0) return dir_size;
    int buffer_size = dir_size + sizeof(size_t) + filename_size;
    if (buffer_size < 0) return buffer_size;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t size;
    memcpy(&size, buffer, sizeof(size_t));
    size++;
    memcpy(buffer, &size, sizeof(size_t));

    memcpy(buffer + dir_size, &file_inode_idx, sizeof(size_t));
    memcpy(buffer + dir_size + sizeof(size_t), filename, filename_size);

    res = file_update(fs, dir_inode_idx, buffer, dir_size + sizeof(size_t) + filename_size, 1);
    free(buffer);
    if (res < 0) return res;

    return 0;
}

int dir_remove_rec(struct FS *fs, size_t dir_inode_idx) {
    int buffer_size = file_size(fs, dir_inode_idx, 1);
    if (buffer_size < 0) return 0;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t i = sizeof(size_t);
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
    int buffer_size = file_size(fs, dir_inode_idx, 1);
    if(buffer_size < 0) return buffer_size;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t i = sizeof(size_t);
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

    size_t size;
    memcpy(&size, buffer, sizeof(size_t));
    size--;
    memcpy(buffer, &size, sizeof(size_t));

    memmove(buffer + i - sizeof(size_t) - filename_size, buffer + i, buffer_size - i);
    res = file_update(fs, dir_inode_idx, buffer, buffer_size - sizeof(size_t) - filename_size, 1);
    free(buffer);
    if (res < 0) return res;

    return 0;
}

int dir_size(struct FS *fs, size_t dir_inode_idx) {
    int size = file_size(fs, dir_inode_idx, 1);
    if (size < 0) return size;

    char* buffer = malloc(size);
    file_read(fs, dir_inode_idx, buffer, size, 1);

    size_t dir_size;
    memcpy(&dir_size, buffer, sizeof(size_t));

    return size - (dir_size + 1) * sizeof(size_t) + dir_size * 46 + 100;
}

int dir_list(struct FS *fs, size_t dir_inode_idx, char *content, size_t content_length) {
    int buffer_size = file_size(fs, dir_inode_idx, 1);
    if (buffer_size < 0) return buffer_size;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t size;
    memcpy(&size, buffer, sizeof(size_t));

    size_t new_content_length = buffer_size - (size + 1) * sizeof(size_t) + size * 46 + 100;
    if (content_length < new_content_length) {
        free(buffer);
        return TOO_SMALL_BUFFER;
    }

    sprintf(content, "Dir %lu, size %lu\nidx\tdir\tsiz\tname\n", dir_inode_idx, size);
    size_t str_offset = strlen(content);

    size_t i = sizeof(size_t);
    while (i < buffer_size) {
        size_t file_inode_idx;
        memcpy(&file_inode_idx, buffer + i, sizeof(size_t));
        int dir_flag = file_is_dir(fs, file_inode_idx);
        int f_size = file_size(fs, file_inode_idx, dir_flag);
        if (f_size < 0) {
            free(buffer);
            return f_size;
        }
        i += sizeof(size_t);

        if (dir_flag == 1) {
            sprintf(content + str_offset, "%lu\t%s\t%d\t", file_inode_idx, "d", f_size);
        } else {
            sprintf(content + str_offset, "%lu\t%s\t%d\t", file_inode_idx, "f", f_size);
        }
        str_offset += strlen(content + str_offset);
        size_t filename_size = strlen(buffer + i) + 1;
        memcpy(content + str_offset, buffer + i, filename_size);
        str_offset += filename_size;
        content[str_offset - 1] = '\n';
        i += filename_size;
    }
    content[str_offset] = '\0';
    free(buffer);
    return 0;
}

int dir_find(struct FS *fs, char *filename, size_t dir_inode_idx) {
    int buffer_size = file_size(fs, dir_inode_idx, 1);
    if (buffer_size < 0) return buffer_size;

    char *buffer = malloc(buffer_size);

    int res = file_read(fs, dir_inode_idx, buffer, buffer_size, 1);
    if (res < 0) {
        free(buffer);
        return res;
    }

    size_t i = sizeof(size_t);
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