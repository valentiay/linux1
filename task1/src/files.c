#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bitmaps.h"
#include "exit_codes.h"
#include "files.h"

int file_fill_with_data(struct FS *fs, size_t inode_idx, size_t *block_idxs, size_t blocks_required, char *content,
                        size_t content_length, size_t dir_flag) {
    // Write to inode block
    fseek(fs->file, fs->inode_table_offset + inode_idx * fs->super_block.inode_size, SEEK_SET);
    if (fwrite(&dir_flag, sizeof(size_t), 1, fs->file) != 1) return WRITE_FAILURE;
    if (fwrite(&content_length, sizeof(size_t), 1, fs->file) != 1) return WRITE_FAILURE;
    if (fwrite(block_idxs, sizeof(size_t), blocks_required, fs->file) != blocks_required) return WRITE_FAILURE;

    // Write to data blocks
    for (size_t i = 0; i < blocks_required - 1; i++) {
        fseek(fs->file, fs->blocks_table_offset + block_idxs[i] * fs->super_block.block_size, SEEK_SET);
        if (fwrite(content + i * fs->super_block.block_size, fs->super_block.block_size, 1, fs->file) != 1)
            return WRITE_FAILURE;
    }
    fseek(fs->file, fs->blocks_table_offset + block_idxs[blocks_required - 1] * fs->super_block.block_size, SEEK_SET);
    if (fwrite(content + (blocks_required - 1) * fs->super_block.block_size,
               content_length % fs->super_block.block_size, 1, fs->file) != 1)
        return WRITE_FAILURE;

    return 0;
}

int file_add(struct FS *fs, char *content, size_t content_length, size_t dir_flag) {
    // Find free inode block
    size_t inode_idx = 0;
    int res = bitmap_find_first_free(fs->file, fs->inode_bitmap_offset, fs->inode_bitmap_length, &inode_idx, 1);
    if (res < 0) return res;

    // Find free data blocks
    size_t blocks_required = content_length / fs->super_block.block_size + 1;
    if (sizeof(size_t) * (blocks_required + 2) > fs->super_block.inode_size) return NO_SPACE;
    fseek(fs->file, fs->blocks_bitmap_offset, SEEK_SET);
    size_t *block_idxs = malloc(sizeof(size_t) * blocks_required);
    res = bitmap_find_first_free(fs->file, fs->blocks_bitmap_offset, fs->blocks_bitmap_length, block_idxs,
                                 blocks_required);
    if (res < 0) {
        free(block_idxs);
        return res;
    }

    // Reserve inode block
    res = bitmap_set(fs->file, fs->inode_bitmap_offset, inode_idx, 1);
    if (res < 0) {
        free(block_idxs);
        return res;
    }

    // Reserve data block
    for (size_t i = 0; i < blocks_required; i++) {
        res = bitmap_set(fs->file, fs->blocks_bitmap_offset, block_idxs[i], 1);
        if (res < 0) {
            free(block_idxs);
            return res;
        }
    }

    res = file_fill_with_data(fs, inode_idx, block_idxs, blocks_required, content, content_length, dir_flag);
    if (res < 0) {
        free(block_idxs);
        return res;
    }
    free(block_idxs);
    return inode_idx;
}

int file_update(struct FS *fs, size_t inode_idx, char *content, size_t new_content_length, size_t dir_flag) {
    int res;

    res = bitmap_read(fs->file, fs->inode_bitmap_offset, inode_idx);
    if (res < 0) return res;
    if (res != 1) return NOT_FOUND;

    size_t content_length;
    size_t old_dir_flag;

    fseek(fs->file, fs->inode_table_offset + inode_idx * fs->super_block.inode_size, SEEK_SET);
    if (fread(&old_dir_flag, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;
    if (old_dir_flag != dir_flag) return WRONG_FILE_TYPE;
    if (fread(&content_length, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;

    size_t blocks_required = content_length / fs->super_block.block_size + 1;
    size_t *block_idxs = malloc(sizeof(size_t) * blocks_required);
    if (fread(block_idxs, sizeof(size_t), blocks_required, fs->file) != blocks_required) {
        free(block_idxs);
        return READ_FAILURE;
    }

    size_t new_blocks_required = new_content_length / fs->super_block.block_size + 1;
    size_t *new_block_idxs = malloc(sizeof(size_t) * new_blocks_required);

    if (new_blocks_required < blocks_required) {
        for (size_t i = new_blocks_required; i < blocks_required; i++) {
            res = bitmap_set(fs->file, fs->blocks_bitmap_offset, block_idxs[i], 0);
            if (res < 0) {
                free(block_idxs);
                free(new_block_idxs);
                return res;
            }
        }
        memcpy(new_block_idxs, block_idxs, sizeof(size_t) * new_blocks_required);
    } else if (new_blocks_required > blocks_required) {
        size_t extra_blocks_required = new_blocks_required - blocks_required;

        if (sizeof(size_t) * (new_blocks_required + 2) > fs->super_block.inode_size) {
            free(block_idxs);
            free(new_block_idxs);
            return NO_SPACE;
        }
        fseek(fs->file, fs->blocks_bitmap_offset, SEEK_SET);
        size_t *extra_block_idxs = malloc(sizeof(size_t) * extra_blocks_required);
        res = bitmap_find_first_free(fs->file, fs->blocks_bitmap_offset, fs->blocks_bitmap_length,
                                     extra_block_idxs, extra_blocks_required);
        if (res < 0) {
            free(block_idxs);
            free(new_block_idxs);
            free(extra_block_idxs);
            return res;
        }

        for (size_t i = 0; i < extra_blocks_required; i++) {
            res = bitmap_set(fs->file, fs->blocks_bitmap_offset, extra_block_idxs[i], 1);
            if (res < 0) {
                free(block_idxs);
                free(new_block_idxs);
                free(extra_block_idxs);
                return res;
            }
        }

        memcpy(new_block_idxs, block_idxs, sizeof(size_t) * blocks_required);
        memcpy(new_block_idxs + blocks_required, extra_block_idxs, sizeof(size_t) * extra_blocks_required);

        free(extra_block_idxs);
    } else {
        memcpy(new_block_idxs, block_idxs, sizeof(size_t) * blocks_required);
    }

    res = file_fill_with_data(fs, inode_idx, new_block_idxs, new_blocks_required, content, new_content_length,
                              dir_flag);
    free(block_idxs);
    free(new_block_idxs);
    if (res < 0) return res;

    return 0;
}

int file_remove(struct FS *fs, size_t inode_idx, size_t dir_flag) {
    size_t new_content_length;
    size_t new_dir_flag;

    fseek(fs->file, fs->inode_table_offset + inode_idx * fs->super_block.inode_size, SEEK_SET);
    if (fread(&new_dir_flag, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;
    if (new_dir_flag != dir_flag) return WRONG_FILE_TYPE;
    if (fread(&new_content_length, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;
    size_t blocks_required = new_content_length / fs->super_block.block_size + 1;
    size_t *block_idxs = malloc(sizeof(size_t) * blocks_required);
    if (fread(block_idxs, sizeof(size_t), blocks_required, fs->file) != blocks_required) {
        free(block_idxs);
        return READ_FAILURE;
    }

    int res = 0;
    for (size_t i = 0; i < blocks_required; i++) {
        res = bitmap_set(fs->file, fs->blocks_bitmap_offset, block_idxs[i], 0);
        if (res < 0) {
            free(block_idxs);
            return res;
        }
    }

    res = bitmap_set(fs->file, fs->inode_bitmap_offset, inode_idx, 0);
    fseek(fs->file, fs->inode_bitmap_offset + inode_idx / 8, SEEK_SET);
    free(block_idxs);
    if (res < 0) return res;
    return 0;
}

int file_is_dir(struct FS* fs, size_t inode_idx) {
    if (bitmap_read(fs->file, fs->inode_bitmap_offset, inode_idx) == 0) return NOT_FOUND;

    size_t new_dir_flag;

    fseek(fs->file, fs->inode_table_offset + inode_idx * fs->super_block.inode_size, SEEK_SET);
    if (fread(&new_dir_flag, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;
    return new_dir_flag;
}

int file_size(struct FS* fs, size_t inode_idx, size_t dir_flag) {
    if (bitmap_read(fs->file, fs->inode_bitmap_offset, inode_idx) == 0) return NOT_FOUND;

    size_t new_dir_flag;

    fseek(fs->file, fs->inode_table_offset + inode_idx * fs->super_block.inode_size, SEEK_SET);
    if (fread(&new_dir_flag, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;
    if (new_dir_flag != dir_flag) return WRONG_FILE_TYPE;

    size_t new_content_length;
    if (fread(&new_content_length, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;
    return new_content_length;
}

int file_read(struct FS *fs, size_t inode_idx, char *content, size_t content_length, size_t dir_flag) {
    size_t new_content_length;
    size_t new_dir_flag;

    if (bitmap_read(fs->file, fs->inode_bitmap_offset, inode_idx) == 0) return NOT_FOUND;

    fseek(fs->file, fs->inode_table_offset + inode_idx * fs->super_block.inode_size, SEEK_SET);
    if (fread(&new_dir_flag, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;
    if (new_dir_flag != dir_flag) return WRONG_FILE_TYPE;
    if (fread(&new_content_length, sizeof(size_t), 1, fs->file) != 1) return READ_FAILURE;
    if (new_content_length > content_length) {
        return TOO_SMALL_BUFFER;
    }
    size_t blocks_required = new_content_length / fs->super_block.block_size + 1;
    size_t *block_idxs = malloc(sizeof(size_t) * blocks_required);
    if (fread(block_idxs, sizeof(size_t), blocks_required, fs->file) != blocks_required) {
        free(block_idxs);
        return READ_FAILURE;
    }

    for (size_t i = 0; i < blocks_required - 1; i++) {
        fseek(fs->file, fs->blocks_table_offset + block_idxs[i] * fs->super_block.block_size, SEEK_SET);
        if (fread(content + i * fs->super_block.block_size, fs->super_block.block_size, 1, fs->file) != 1) {
            free(block_idxs);
            return READ_FAILURE;
        }
    }
    fseek(fs->file, fs->blocks_table_offset + block_idxs[blocks_required - 1] * fs->super_block.block_size, SEEK_SET);
    if (fread(content + (blocks_required - 1) * fs->super_block.block_size,
              new_content_length % fs->super_block.block_size, 1, fs->file) != 1) {
        free(block_idxs);
        return READ_FAILURE;
    }

    free(block_idxs);
    return 0;
}
