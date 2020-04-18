#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "exit_codes.h"
#include "dirs.h"

struct SuperBlock init_default_super_block() {
    struct SuperBlock super_block = {
            1024,
            256,
            1024,
            256,
            1024,
            128
    };
    return super_block;
}

int fs_add(struct FS *fs, char *path, char *content, size_t content_length) {
    size_t current_dir_inode_idx = 0;
    size_t word_start = 1;
    size_t path_length = strlen(path);
    int res;

    if (path[0] != '/') return NOT_FOUND;
    for (size_t i = 1; i < path_length; i++) {
        if (path[i] == '/') {
            if (i - word_start == 0) return NOT_FOUND;
            char *dirname = malloc(i - word_start + 1);
            memcpy(dirname, path + word_start, i - word_start);
            dirname[i - word_start] = '\0';
            printf("%s\n", dirname);
            res = dir_find(fs, dirname, current_dir_inode_idx);
            if (res == NOT_FOUND) {
                res = dir_init(fs);
                if (res >= 0) {
                    int res2 = dir_add(fs, dirname, res, current_dir_inode_idx);
                    if (res2 < 0) {
                        free(dirname);
                        return res;
                    }
                }
            }
            free(dirname);
            if (res < 0) return res;
            current_dir_inode_idx = res;
            word_start = i + 1;
        }
    }

    if (path[path_length - 1] != '/') {
        res = file_add(fs, content, content_length, 0);
        if (res < 0) return res;
        res = dir_add(fs, path + word_start, res, current_dir_inode_idx);
        if (res < 0) return res;
    }

    return 0;
}

int fs_update(struct FS *fs, char *path, char *content, size_t content_length) {
    size_t current_dir_inode_idx = 0;
    size_t word_start = 1;
    size_t path_length = strlen(path);
    int res;

    if (path[0] != '/') return NOT_FOUND;
    for (size_t i = 1; i < path_length; i++) {
        if (path[i] == '/') {
            if (i - word_start == 0) return NOT_FOUND;
            char *dirname = malloc(i - word_start + 1);
            memcpy(dirname, path + word_start, i - word_start);
            dirname[i - word_start] = '\0';
            printf("%s\n", dirname);
            res = dir_find(fs, dirname, current_dir_inode_idx);
            if (res == NOT_FOUND) {
                res = dir_init(fs);
                if (res >= 0) {
                    int res2 = dir_add(fs, dirname, res, current_dir_inode_idx);
                    if (res2 < 0) {
                        free(dirname);
                        return res2;
                    }
                }
            }
            free(dirname);
            if (res < 0) return res;
            current_dir_inode_idx = res;
            word_start = i + 1;
        }
    }

    if (path[path_length - 1] != '/') {
        int file_inode_idx = dir_find(fs, path + word_start, current_dir_inode_idx);
        if (file_inode_idx < 0) return file_inode_idx;
        res = file_update(fs, file_inode_idx, content, content_length, 0);
        if (res < 0) return res;
        res = dir_add(fs, path + word_start, res, current_dir_inode_idx);
        if (res < 0) return res;
    }

    return 0;
}

//int fs_size(struct FS *fs, char *path) {
//    size_t current_dir_inode_idx = 0;
//    size_t word_start = 1;
//    size_t path_length = strlen(path);
//    int res;
//
//    if (path[0] != '/') return NOT_FOUND;
//    for (size_t i = 1; i < path_length; i++) {
//        if (path[i] == '/') {
//            if (i - word_start == 0) return NOT_FOUND;
//            char *dirname = malloc(i - word_start + 1);
//            memcpy(dirname, path + word_start, i - word_start);
//            dirname[i - word_start] = '\0';
//            printf("%s\n", dirname);
//            res = dir_find(fs, dirname, current_dir_inode_idx);
//            free(dirname);
//            if (res < 0) return res;
//            current_dir_inode_idx = res;
//            word_start = i + 1;
//        }
//    }
//
//    if (path[path_length - 1] != '/') {
//        return file_size(fs, current_dir_inode_idx);
//    } else {
//        return dir_size(fs, current_dir_inode_idx);
//    }
//}

int fs_read(struct FS *fs, char *path) {
    size_t current_dir_inode_idx = 0;
    size_t word_start = 1;
    size_t path_length = strlen(path);
    int res;

    if (path[0] != '/') return NOT_FOUND;
    for (size_t i = 1; i < path_length; i++) {
        if (path[i] == '/') {
            if (i - word_start == 0) return NOT_FOUND;
            char *dirname = malloc(i - word_start + 1);
            memcpy(dirname, path + word_start, i - word_start);
            dirname[i - word_start] = '\0';
            printf("%s\n", dirname);
            res = dir_find(fs, dirname, current_dir_inode_idx);
            free(dirname);
            if (res < 0) return res;
            current_dir_inode_idx = res;
            word_start = i + 1;
        }
    }

    if (path[path_length - 1] != '/') {
        int buffer_size = file_size(fs, current_dir_inode_idx);
        if (buffer_size < 0) return buffer_size;
        char *buffer = malloc(buffer_size);

        size_t file_inode_idx = dir_find(fs, path + word_start, current_dir_inode_idx);
        res = file_read(fs, file_inode_idx, buffer, buffer_size, 0);
        if (res < 0) {
            free(buffer);
            return res;
        }
        buffer[buffer_size - 1] = '\0';

        printf("File (%lu):\n%s\n", file_inode_idx, buffer);

        free(buffer);
    } else {
        printf("Dir (%lu):\n", current_dir_inode_idx);
        dir_list(fs, current_dir_inode_idx);
    }

    return 0;
}

int fs_remove(struct FS *fs, char *path) {
    if (strcmp(path, "/") == 0) return WRONG_INPUT;

    size_t prev_dir_inode_idx = 0;
    size_t current_dir_inode_idx = 0;
    size_t prev_word_start = 1;
    size_t word_start = 1;
    size_t path_length = strlen(path);
    int res;

    if (path[0] != '/') return NOT_FOUND;
    for (size_t i = 1; i < path_length; i++) {
        if (path[i] == '/') {
            if (i - word_start == 0) return NOT_FOUND;
            char *dirname = malloc(i - word_start + 1);
            memcpy(dirname, path + word_start, i - word_start);
            dirname[i - word_start] = '\0';
            printf("%s\n", dirname);
            res = dir_find(fs, dirname, current_dir_inode_idx);
            free(dirname);
            if (res < 0) return res;
            prev_dir_inode_idx = current_dir_inode_idx;
            current_dir_inode_idx = res;
            prev_word_start = word_start;
            word_start = i + 1;
        }
    }

    if (path[path_length - 1] == '/') {
        char *dir_name = malloc(path_length - prev_word_start + 1);
        memcpy(dir_name, path + prev_word_start, path_length - prev_word_start - 1);
        dir_name[path_length - prev_word_start - 1] = '\0';
        res = dir_remove_file(fs, dir_name, prev_dir_inode_idx);
        free(dir_name);
    } else {
        res = dir_remove_file(fs, path + word_start, current_dir_inode_idx);
    }
    if (res < 0) return res;
    return 0;
}

int fs_init(FILE *file) {
    struct SuperBlock super_block = init_default_super_block();
    fwrite(&super_block, sizeof(struct SuperBlock), 1, file);

    size_t inode_bitmap_length = super_block.inodes_number / 8 + 1;
    size_t blocks_bitmap_length = super_block.blocks_number / 8 + 1;
    size_t inode_table_length = super_block.inode_size * super_block.inodes_number;
    size_t blocks_table_length = super_block.block_size * super_block.blocks_number;

    size_t total_length = inode_bitmap_length + blocks_bitmap_length + inode_table_length + blocks_table_length;

    char *bytes = malloc(total_length);
    memset(bytes, 0, total_length);
    fwrite(bytes, total_length, 1, file);
    free(bytes);

    if (dir_init(file) != 0) return WRITE_FAILURE;
    return 0;
}

int fs_open(struct FS *fs, char *filename) {
    FILE *file;
    if (access(filename, F_OK) != -1) {
        file = fopen(filename, "rb+");
        if (file == NULL) return READ_FAILURE;
    } else {
        file = fopen(filename, "wb+");
        if (file == NULL) return READ_FAILURE;
        int res = fs_init(file);
        if (res < 0) return res;
    }

    fs->file = file;

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;
    fs->super_block = super_block;

    fs->inode_bitmap_length = super_block.inodes_number / 8 + 1;
    fs->blocks_bitmap_length = super_block.blocks_number / 8 + 1;
    fs->inode_table_length = super_block.inode_size * super_block.inodes_number;
    fs->blocks_table_length = super_block.block_size * super_block.blocks_number;

    fs->inode_bitmap_offset = sizeof(struct SuperBlock);
    fs->blocks_bitmap_offset = fs->inode_bitmap_offset + fs->inode_bitmap_length;
    fs->inode_table_offset = fs->blocks_bitmap_offset + fs->blocks_bitmap_length;
    fs->blocks_table_offset = fs->inode_table_offset + fs->inode_table_length;

    return 0;
}

int close_fs(struct FS *fs) {
    if (fclose(fs->file) < 0) return WRITE_FAILURE;
    return 0;
}

void dump_super_block(struct SuperBlock *super_block) {
    printf(
            "SuperBlock:%lu,%lu,%lu,%lu,%lu,%lu",
            super_block->blocks_number,
            super_block->inodes_number,
            super_block->free_blocks_number,
            super_block->free_inodes_number,
            super_block->block_size,
            super_block->inode_size
    );
}

int dump_bitmaps(FILE *file) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t inode_bitmap_length = super_block.inodes_number / 8 + 1;
    size_t blocks_bitmap_length = super_block.blocks_number / 8 + 1;

    char byte;

    printf("\nInode bitmap:\n");
    for (size_t byte_idx = 0; byte_idx < inode_bitmap_length; byte_idx++) {
        fread(&byte, 1, 1, file);
        for (size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            printf("%d", (byte >> bit_idx) & 1);
        }
        printf(" ");
    }
    printf("\nBlock bitmap:\n");
    for (size_t byte_idx = 0; byte_idx < blocks_bitmap_length; byte_idx++) {
        fread(&byte, 1, 1, file);
        for (size_t bit_idx = 0; bit_idx < 8; bit_idx++) {
            printf("%d", (byte >> bit_idx) & 1);
        }
        printf(" ");
    }
    printf("\n");
    return 0;
}