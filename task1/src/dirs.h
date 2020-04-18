#ifndef TASK1_DIRS_H
#define TASK1_DIRS_H

#include "files.h"

int dir_init(struct FS *fs);

int dir_add(struct FS *fs, char *filename, size_t file_inode_idx, size_t dir_inode_idx);

int dir_remove_rec(struct FS *fs, size_t dir_inode_idx);

int dir_remove_file(struct FS *fs, char *filename, size_t dir_inode_idx);

int dir_size(struct FS *fs, size_t dir_inode_idx);

int dir_list(struct FS *fs, size_t dir_inode_idx, char *content, size_t content_length);

int dir_find(struct FS *fs, char *filename, size_t dir_inode_idx);

#endif //TASK1_DIRS_H
