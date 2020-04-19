#ifndef TASK1_FS_H
#define TASK1_FS_H

#include <stdio.h>
#include <zconf.h>

#include "exit_codes.h"
#include "files.h"

struct SuperBlock init_default_super_block();

int fs_add(struct FS *fs, char *path, char *content, size_t content_length);

int fs_update(struct FS *fs, char *path, char *content, size_t content_length);

int fs_size(struct FS *fs, char *path);

int fs_read(struct FS *fs, char *path, char *content, size_t content_length);

int fs_remove(struct FS *fs, char *path);

int fs_init(struct FS *file);

int fs_open(struct FS *fs, char *filename);

int fs_close(struct FS *fs);

void dump_super_block(struct SuperBlock *super_block);

int dump_bitmaps(FILE *file);

#endif //TASK1_FS_H
