#ifndef TASK1_FILES_H
#define TASK1_FILES_H

#include <stdio.h>

struct SuperBlock {
    size_t blocks_number;
    size_t inodes_number;
    size_t free_blocks_number;
    size_t free_inodes_number;
    size_t block_size;
    size_t inode_size;
};

struct FS {
    FILE *file;
    struct SuperBlock super_block;
    size_t inode_bitmap_length;
    size_t blocks_bitmap_length;
    size_t inode_table_length;
    size_t blocks_table_length;

    size_t inode_bitmap_offset;
    size_t blocks_bitmap_offset;
    size_t inode_table_offset;
    size_t blocks_table_offset;
};

int file_fill_with_data(struct FS *fs, size_t inode_idx, size_t *block_idxs, size_t blocks_required, char *content,
                        size_t content_length, size_t dir_flag);

int file_add(struct FS *fs, char *content, size_t content_length, size_t dir_flag);

int file_update(struct FS *fs, size_t inode_idx, char *content, size_t new_content_length, size_t dir_flag);

int file_remove(struct FS *fs, size_t inode_idx, size_t dir_flag);

int file_size(struct FS* fs, size_t inode_idx);

int file_read(struct FS *fs, size_t inode_idx, char *content, size_t content_length, size_t dir_flag);

#endif //TASK1_FILES_H
