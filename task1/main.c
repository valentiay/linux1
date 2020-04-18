#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_SPACE -1
#define READ_FAILURE -2
#define WRITE_FAILURE -3
#define TOO_SMALL_BUFFER -4
#define WRONG_FILE_TYPE -5
#define NOT_FOUND -6
#define WRONG_INPUT -7

struct SuperBlock {
    size_t blocks_number;
    size_t inodes_number;
    size_t free_blocks_number;
    size_t free_inodes_number;
    size_t block_size;
    size_t inode_size;
};

struct FS {
    FILE* file;
    struct SuperBlock super_block;
    size_t inode_bitmap_length;
    size_t blocks_bitmap_length;
    size_t inode_table_length;
    size_t blocks_table_length;
};

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

int bitmap_set(FILE* file, size_t offset, size_t idx, char value) {
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

int bitmap_read(FILE* file, size_t offset, size_t idx) {
    fseek(file, offset + idx / 8, SEEK_SET);
    char byte;
    if (fread(&byte, 1, 1, file) != 1) return READ_FAILURE;
    return (byte >> (idx % 8)) & 1;
}

int bitmap_find_first_free(FILE* file, size_t offset, size_t bitmap_length, size_t* results, size_t required) {
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


int fill_with_data(struct SuperBlock* super_block, FILE* file, size_t inode_idx, size_t* block_idxs, size_t blocks_required, char* content, size_t content_length, size_t dir_flag) {
    size_t inode_bitmap_length = super_block->inodes_number / 8 + 1;
    size_t blocks_bitmap_length = super_block->blocks_number / 8 + 1;
    size_t inode_table_length = super_block->inode_size * super_block->inodes_number;

    // Write to inode block
    size_t inodes_offset = sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length;
    fseek(file, inodes_offset + inode_idx * super_block->inode_size, SEEK_SET);
    if (fwrite(&dir_flag, sizeof(size_t), 1, file) != 1) return WRITE_FAILURE;
    if (fwrite(&content_length, sizeof(size_t), 1, file) != 1) return WRITE_FAILURE;
    if (fwrite(block_idxs, sizeof(size_t), blocks_required, file) != blocks_required) return WRITE_FAILURE;

    // Write to data blocks
    size_t blocks_offset = sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_table_length;
    for (size_t i = 0; i < blocks_required - 1; i++) {
        fseek(file,  blocks_offset + block_idxs[i] * super_block->block_size, SEEK_SET);
        if (fwrite(content + i * super_block->block_size, super_block->block_size, 1, file) != 1) return WRITE_FAILURE;
    }
    fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_table_length + block_idxs[blocks_required - 1]  * super_block->block_size, SEEK_SET);
    if (fwrite(content + (blocks_required - 1) * super_block->block_size, content_length % super_block->block_size, 1, file) != 1) return WRITE_FAILURE;

    return 0;
}

int add_file(FILE* file, char* content, size_t content_length, size_t dir_flag) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t inode_bitmap_length = super_block.inodes_number / 8 + 1;
    size_t blocks_bitmap_length = super_block.blocks_number / 8 + 1;
    size_t inode_table_length = super_block.inode_size * super_block.inodes_number;
    size_t blocks_table_length = super_block.block_size * super_block.blocks_number;

    // Find free inode block
    size_t inode_idx = 0;
    int bitmap_res = bitmap_find_first_free(file, sizeof(struct SuperBlock), inode_bitmap_length, &inode_idx, 1);
    if (bitmap_res < 0) return bitmap_res;

    // Find free data blocks
    size_t blocks_required = content_length / super_block.block_size + 1;
    if (sizeof(size_t) * (blocks_required + 2) > super_block.inode_size) return NO_SPACE;
    fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length, SEEK_SET);
    size_t* block_idxs = malloc(sizeof(size_t) * blocks_required);
    bitmap_res = bitmap_find_first_free(file, sizeof(struct SuperBlock) + inode_bitmap_length, blocks_bitmap_length, block_idxs, blocks_required);
    if (bitmap_res < 0) return bitmap_res;

    // Reserve inode block
    bitmap_res = bitmap_set(file, sizeof(struct SuperBlock), inode_idx, 1);
    if (bitmap_res < 0) return bitmap_res;

    // Reserve data block
    for (size_t i = 0; i < blocks_required; i++) {
        bitmap_res = bitmap_set(file, sizeof(struct SuperBlock) + inode_bitmap_length, block_idxs[i], 1);
        if (bitmap_res < 0) return bitmap_res;
    }

//    // Write to inode block
//    fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_idx * super_block.inode_size, SEEK_SET);
//    if (fwrite(&dir_flag, sizeof(size_t), 1, file) != 1) return WRITE_FAILURE;
//    if (fwrite(&content_length, sizeof(size_t), 1, file) != 1) return WRITE_FAILURE;
//    if (fwrite(block_idxs, sizeof(size_t), blocks_required, file) != blocks_required) return WRITE_FAILURE;
//
//    // Write to data blocks
//    for (size_t i = 0; i < blocks_required - 1; i++) {
//        fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_table_length + block_idxs[i] * super_block.block_size, SEEK_SET);
//        if (fwrite(content + i * super_block.block_size, super_block.block_size, 1, file) != 1) return WRITE_FAILURE;
//    }
//    fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_table_length + block_idxs[blocks_required - 1]  * super_block.block_size, SEEK_SET);
//    if (fwrite(content + (blocks_required - 1) * super_block.block_size, content_length % super_block.block_size, 1, file) != 1) return WRITE_FAILURE;

    fill_with_data(&super_block, file, inode_idx, block_idxs, blocks_required, content, content_length, dir_flag);

    free(block_idxs);
    return inode_idx;
}

int update_file(FILE* file, size_t inode_idx, char* content, size_t new_content_length, size_t dir_flag) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t inode_bitmap_length = super_block.inodes_number / 8 + 1;
    size_t blocks_bitmap_length = super_block.blocks_number / 8 + 1;
    size_t inode_table_length = super_block.inode_size * super_block.inodes_number;
    size_t blocks_table_length = super_block.block_size * super_block.blocks_number;

    int bitmap_res;

    bitmap_res = bitmap_read(file, sizeof(struct SuperBlock), inode_idx);
    if (bitmap_res < 0) return bitmap_res;
    if (bitmap_res != 1) return NOT_FOUND;

    size_t content_length;
    size_t old_dir_flag;

    fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_idx * super_block.inode_size, SEEK_SET);
    if (fread(&old_dir_flag, sizeof(size_t), 1, file) != 1) return READ_FAILURE;
    if (old_dir_flag != dir_flag) return WRONG_FILE_TYPE;
    if (fread(&content_length, sizeof(size_t), 1, file) != 1) return READ_FAILURE;

    size_t blocks_required = content_length / super_block.block_size + 1;
    size_t* block_idxs = malloc(sizeof(size_t) * blocks_required);
    if (fread(block_idxs, sizeof(size_t), blocks_required, file) != blocks_required) return READ_FAILURE;

//    printf("\nblock_idxs:\n");
//    for (size_t i = 0; i < blocks_required; i++) {
//        printf("%lu ", block_idxs[i]);
//    }

    size_t new_blocks_required = new_content_length / super_block.block_size + 1;
    size_t* new_block_idxs = malloc(sizeof(size_t) * new_blocks_required);

    if (new_blocks_required < blocks_required) {
        for (size_t i = new_blocks_required; i < blocks_required; i++) {
            bitmap_res = bitmap_set(file, sizeof(struct SuperBlock) + inode_bitmap_length, block_idxs[i], 0);
            if (bitmap_res < 0) return bitmap_res;
        }
        memcpy(new_block_idxs, block_idxs, sizeof(size_t) * new_blocks_required);
    } else if (new_blocks_required > blocks_required) {
        size_t extra_blocks_required = new_blocks_required - blocks_required;

        if (sizeof(size_t) * (new_blocks_required + 2) > super_block.inode_size) return NO_SPACE;
        fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length, SEEK_SET);
        size_t* extra_block_idxs = malloc(sizeof(size_t) * extra_blocks_required);
        bitmap_res = bitmap_find_first_free(file, sizeof(struct SuperBlock) + inode_bitmap_length, blocks_bitmap_length, extra_block_idxs, extra_blocks_required);
        if (bitmap_res < 0) return bitmap_res;

        for (size_t i = 0; i < extra_blocks_required; i++) {
            bitmap_res = bitmap_set(file, sizeof(struct SuperBlock) + inode_bitmap_length, extra_block_idxs[i], 1);
            if (bitmap_res < 0) return bitmap_res;
        }

        memcpy(new_block_idxs, block_idxs, sizeof(size_t) * blocks_required);
        memcpy(new_block_idxs + blocks_required, extra_block_idxs, sizeof(size_t) * extra_blocks_required);

        free(extra_block_idxs);
    } else {
        memcpy(new_block_idxs, block_idxs, sizeof(size_t) * blocks_required);
    }

//    printf("\nOLD:\n");
//    for (size_t i = 0; i < blocks_required; i++) {
//        printf("%lu ", block_idxs[i]);
//    }
//    printf("\nNEW:\n");
//    for (size_t i = 0; i < new_blocks_required; i++) {
//        printf("%lu ", new_block_idxs[i]);
//    }
//    printf("\n");

    bitmap_res = fill_with_data(&super_block, file, inode_idx, new_block_idxs, new_blocks_required, content, new_content_length, dir_flag);
    if (bitmap_res < 0) return bitmap_res;

    free(block_idxs);
    free(new_block_idxs);

    return 0;
}

int remove_file(FILE* file, size_t inode_idx, size_t dir_flag) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t inode_bitmap_length = super_block.inodes_number / 8 + 1;
    size_t blocks_bitmap_length = super_block.blocks_number / 8 + 1;
    size_t inode_table_length = super_block.inode_size * super_block.inodes_number;
    size_t blocks_table_length = super_block.block_size * super_block.blocks_number;

    size_t new_content_length;
    size_t new_dir_flag;

    fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_idx * super_block.inode_size, SEEK_SET);
    if (fread(&new_dir_flag, sizeof(size_t), 1, file) != 1) return READ_FAILURE;
    if (new_dir_flag != dir_flag) return WRONG_FILE_TYPE;
    if (fread(&new_content_length, sizeof(size_t), 1, file) != 1) return READ_FAILURE;
    size_t blocks_required = new_content_length / super_block.block_size + 1;
    size_t* block_idxs = malloc(sizeof(size_t) * blocks_required);
    if (fread(block_idxs, sizeof(size_t), blocks_required, file) != blocks_required) return READ_FAILURE;

    int bitmap_res = 0;
    for (size_t i = 0; i < blocks_required; i++) {
        bitmap_res = bitmap_set(file, sizeof(struct SuperBlock) + inode_bitmap_length, block_idxs[i], 0);
        if (bitmap_res < 0) return bitmap_res;
    }

    bitmap_res = bitmap_set(file, sizeof(struct SuperBlock), inode_idx, 0);
    fseek(file, sizeof(struct SuperBlock) + inode_idx / 8, SEEK_SET);
    free(block_idxs);
    if (bitmap_res < 0) return bitmap_res;
    return 0;
}

int read_file(FILE* file, size_t inode_idx, char* content, size_t* content_length, size_t dir_flag) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t inode_bitmap_length = super_block.inodes_number / 8 + 1;
    size_t blocks_bitmap_length = super_block.blocks_number / 8 + 1;
    size_t inode_table_length = super_block.inode_size * super_block.inodes_number;
    size_t blocks_table_length = super_block.block_size * super_block.blocks_number;

    size_t new_content_length;
    size_t new_dir_flag;

    if (bitmap_read(file, sizeof(struct SuperBlock), inode_idx) == 0) return NOT_FOUND;

    fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_idx * super_block.inode_size, SEEK_SET);
    if (fread(&new_dir_flag, sizeof(size_t), 1, file) != 1) return READ_FAILURE;
    if (new_dir_flag != dir_flag) return WRONG_FILE_TYPE;
    if (fread(&new_content_length, sizeof(size_t), 1, file) != 1) return READ_FAILURE;
    if (new_content_length > *content_length) {
        *content_length = new_content_length;
        return TOO_SMALL_BUFFER;
    }
    size_t blocks_required = new_content_length / super_block.block_size + 1;
    size_t* block_idxs = malloc(sizeof(size_t) * blocks_required);
    if (fread(block_idxs, sizeof(size_t), blocks_required, file) != blocks_required) return READ_FAILURE;

    for (size_t i = 0; i < blocks_required - 1; i++) {
        fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_table_length + block_idxs[i] * super_block.block_size, SEEK_SET);
        if (fread(content + i * super_block.block_size, super_block.block_size, 1, file) != 1) return READ_FAILURE;
    }
    fseek(file, sizeof(struct SuperBlock) + inode_bitmap_length + blocks_bitmap_length + inode_table_length + block_idxs[blocks_required - 1] * super_block.block_size, SEEK_SET);
    if (fread(content + (blocks_required - 1) * super_block.block_size, new_content_length % super_block.block_size, 1, file) != 1) return READ_FAILURE;

    *content_length = new_content_length;

    free(block_idxs);
    return 0;
}


int init_empty_dir(FILE* file) {
    char content = 'e';
    return add_file(file, &content, 1, 1);
}

int add_file_to_dir(FILE* file, char* filename, size_t file_inode_idx, size_t dir_inode_idx) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t filename_size = strlen(filename) + 1;

    size_t buffer_size = super_block.block_size + sizeof(size_t) + filename_size;
    char* buffer = malloc(buffer_size);

    int bitmap_res;
    bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
    if (bitmap_res == TOO_SMALL_BUFFER) {
        free(buffer);
        buffer = malloc(buffer_size + sizeof(size_t) + filename_size);
        bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
        if (bitmap_res < 0) return bitmap_res;
    } else if (bitmap_res < 0) return bitmap_res;

    memcpy(buffer + buffer_size, &file_inode_idx, sizeof(size_t));
    memcpy(buffer + buffer_size + sizeof(size_t), filename, filename_size);

    bitmap_res = update_file(file, dir_inode_idx, buffer, buffer_size + sizeof(size_t) + filename_size, 1);
    if (bitmap_res < 0) return bitmap_res;

    free(buffer);
    return 0;
}

int remove_dir(FILE* file, size_t dir_inode_idx) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t buffer_size = super_block.block_size;
    char* buffer = malloc(buffer_size);

    int bitmap_res;
    bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
    if (bitmap_res == TOO_SMALL_BUFFER) {
        free(buffer);
        buffer = malloc(buffer_size);
        bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
        if (bitmap_res < 0) return bitmap_res;
    } else if (bitmap_res < 0) return bitmap_res;

    size_t i = 1;
    while (i < buffer_size) {
        size_t file_inode_idx;
        memcpy(&file_inode_idx, buffer + i, sizeof(size_t));
        i += sizeof(size_t);
        bitmap_res = remove_file(file, file_inode_idx, 0);
        if (bitmap_res == WRONG_FILE_TYPE) {
            remove_dir(file, file_inode_idx);
        }
        if (bitmap_res < 0) return bitmap_res;

        size_t filename_size = strlen(buffer + i) + 1;
        i += filename_size;
    }
    remove_file(file, dir_inode_idx, 1);
    free(buffer);
    return 0;
}

int remove_file_from_dir(FILE* file, char* filename, size_t dir_inode_idx) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t buffer_size = super_block.block_size;
    char* buffer = malloc(buffer_size);

    int bitmap_res;
    bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
    if (bitmap_res == TOO_SMALL_BUFFER) {
        free(buffer);
        buffer = malloc(buffer_size);
        bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
        if (bitmap_res < 0) return bitmap_res;
    } else if (bitmap_res < 0) return bitmap_res;

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

    if (found == 0) return NOT_FOUND;

    bitmap_res = remove_file(file, file_inode_idx, 0);
    if (bitmap_res == WRONG_FILE_TYPE) {
        bitmap_res = remove_dir(file, file_inode_idx);
    }
    if (bitmap_res < 0) return bitmap_res;

    memmove(buffer + i - sizeof(size_t) - filename_size, buffer + i, buffer_size - i);
    update_file(file, dir_inode_idx, buffer, buffer_size - sizeof(size_t) - filename_size, 1);

    free(buffer);

    return 0;
}

int list_dir(FILE* file, size_t dir_inode_idx) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t buffer_size = super_block.block_size;
    char* buffer = malloc(buffer_size);

    int bitmap_res;
    bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
    if (bitmap_res == TOO_SMALL_BUFFER) {
        free(buffer);
        buffer = malloc(buffer_size);
        bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
        if (bitmap_res < 0) return bitmap_res;
    } else if (bitmap_res < 0) return bitmap_res;

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

int find_file_in_dir(FILE* file, char* filename, size_t dir_inode_idx) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t buffer_size = super_block.block_size;
    char* buffer = malloc(buffer_size);

    int bitmap_res;
    bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
    if (bitmap_res == TOO_SMALL_BUFFER) {
        free(buffer);
        buffer = malloc(buffer_size);
        bitmap_res = read_file(file, dir_inode_idx, buffer, &buffer_size, 1);
        if (bitmap_res < 0) return bitmap_res;
    } else if (bitmap_res < 0) return bitmap_res;

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


int add_file_by_name(FILE* file, char* path, char* content, size_t content_length) {
    size_t current_dir_inode_idx = 0;
    size_t word_start = 1;
    size_t path_length = strlen(path);
    int bitmap_res;

    if (path[0] != '/') return NOT_FOUND;
    for(size_t i = 1; i < path_length; i++) {
        if (path[i] == '/') {
            if (i - word_start == 0) return NOT_FOUND;
            char* dirname = malloc(i - word_start + 1);
            memcpy(dirname, path + word_start, i - word_start);
            dirname[i - word_start] = '\0';
            printf("%s\n", dirname);
            bitmap_res = find_file_in_dir(file, dirname, current_dir_inode_idx);
            if (bitmap_res == NOT_FOUND) {
                bitmap_res = init_empty_dir(file);
                if (bitmap_res >= 0) {
                    int bitmap_res2 = add_file_to_dir(file, dirname, bitmap_res, current_dir_inode_idx);
                    if (bitmap_res2 < 0) return bitmap_res;
                }
            }
            if (bitmap_res < 0) return bitmap_res;
            current_dir_inode_idx = bitmap_res;
            free(dirname);
            word_start = i + 1;
        }
    }

    if (path[path_length - 1] != '/') {
        bitmap_res = add_file(file, content, content_length, 0);
        if (bitmap_res < 0) return bitmap_res;
        bitmap_res = add_file_to_dir(file, path + word_start, bitmap_res, current_dir_inode_idx);
        if (bitmap_res < 0) return bitmap_res;
    }
    printf("Found dir: %lu\n", current_dir_inode_idx);

    return 0;
}

int update_file_by_name(FILE* file, char* path, char* content, size_t content_length) {
    size_t current_dir_inode_idx = 0;
    size_t word_start = 1;
    size_t path_length = strlen(path);
    int bitmap_res;

    if (path[0] != '/') return NOT_FOUND;
    for(size_t i = 1; i < path_length; i++) {
        if (path[i] == '/') {
            if (i - word_start == 0) return NOT_FOUND;
            char* dirname = malloc(i - word_start + 1);
            memcpy(dirname, path + word_start, i - word_start);
            dirname[i - word_start] = '\0';
            printf("%s\n", dirname);
            bitmap_res = find_file_in_dir(file, dirname, current_dir_inode_idx);
            if (bitmap_res == NOT_FOUND) {
                bitmap_res = init_empty_dir(file);
                if (bitmap_res >= 0) {
                    int bitmap_res2 = add_file_to_dir(file, dirname, bitmap_res, current_dir_inode_idx);
                    if (bitmap_res2 < 0) return bitmap_res;
                }
            }
            if (bitmap_res < 0) return bitmap_res;
            current_dir_inode_idx = bitmap_res;
            free(dirname);
            word_start = i + 1;
        }
    }

    if (path[path_length - 1] != '/') {
        int file_inode_idx = find_file_in_dir(file, path + word_start, current_dir_inode_idx);
        if (file_inode_idx < 0) return file_inode_idx;
        bitmap_res = update_file(file,  file_inode_idx, content, content_length, 0);
        if (bitmap_res < 0) return bitmap_res;
        bitmap_res = add_file_to_dir(file, path + word_start, bitmap_res, current_dir_inode_idx);
        if (bitmap_res < 0) return bitmap_res;
    }

    return 0;
}

int list_by_name(FILE* file, char* path) {
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t current_dir_inode_idx = 0;
    size_t word_start = 1;
    size_t path_length = strlen(path);
    int bitmap_res;

    if (path[0] != '/') return NOT_FOUND;
    for(size_t i = 1; i < path_length; i++) {
        if (path[i] == '/') {
            if (i - word_start == 0) return NOT_FOUND;
            char* dirname = malloc(i - word_start + 1);
            memcpy(dirname, path + word_start, i - word_start);
            dirname[i - word_start] = '\0';
            printf("%s\n", dirname);
            bitmap_res = find_file_in_dir(file, dirname, current_dir_inode_idx);
            free(dirname);
            if (bitmap_res < 0) return bitmap_res;
            current_dir_inode_idx = bitmap_res;
            word_start = i + 1;
        }
    }

    if (path[path_length - 1] != '/') {
        size_t buffer_size = super_block.block_size + 1;
        char* buffer = malloc(buffer_size);

        size_t file_inode_idx = find_file_in_dir(file, path + word_start, current_dir_inode_idx);
        bitmap_res = read_file(file, file_inode_idx, buffer, &buffer_size, 0);
        if (bitmap_res == TOO_SMALL_BUFFER) {
            free(buffer);
            buffer_size++;
            buffer = malloc(buffer_size);
            bitmap_res = read_file(file, file_inode_idx, buffer, &buffer_size, 0);
        }
        if (bitmap_res < 0) return bitmap_res;
        buffer[buffer_size - 1] = '\0';

        printf("File (%lu):\n%s\n", file_inode_idx, buffer);

        free(buffer);
    } else {
        printf("Dir (%lu):\n", current_dir_inode_idx);
        list_dir(file, current_dir_inode_idx);
    }

    return 0;
}

int remove_by_name(FILE* file, char* path) {
    if (strcmp(path, "/") == 0) return WRONG_INPUT;
    fseek(file, 0, SEEK_SET);

    struct SuperBlock super_block;
    if (fread(&super_block, sizeof(struct SuperBlock), 1, file) != 1) return READ_FAILURE;

    size_t prev_dir_inode_idx = 0;
    size_t current_dir_inode_idx = 0;
    size_t prev_word_start = 1;
    size_t word_start = 1;
    size_t path_length = strlen(path);
    int bitmap_res;

    if (path[0] != '/') return NOT_FOUND;
    for(size_t i = 1; i < path_length; i++) {
        if (path[i] == '/') {
            if (i - word_start == 0) return NOT_FOUND;
            char* dirname = malloc(i - word_start + 1);
            memcpy(dirname, path + word_start, i - word_start);
            dirname[i - word_start] = '\0';
            printf("%s\n", dirname);
            bitmap_res = find_file_in_dir(file, dirname, current_dir_inode_idx);
            free(dirname);
            if (bitmap_res < 0) return bitmap_res;
            prev_dir_inode_idx = current_dir_inode_idx;
            current_dir_inode_idx = bitmap_res;
            prev_word_start = word_start;
            word_start = i + 1;
        }
    }

    if (path[path_length - 1] == '/') {
        char* dir_name = malloc(path_length - prev_word_start + 1);
        memcpy(dir_name, path + prev_word_start, path_length - prev_word_start - 1);
        dir_name[path_length - prev_word_start - 1] = '\0';
        bitmap_res = remove_file_from_dir(file, dir_name, prev_dir_inode_idx);
        free(dir_name);
    } else {
        bitmap_res = remove_file_from_dir(file, path + word_start, current_dir_inode_idx);
    }
    if (bitmap_res < 0) return bitmap_res;
    return 0;
}

int init_fs(FILE* file) {
    struct SuperBlock super_block = init_default_super_block();
    fwrite(&super_block, sizeof(struct SuperBlock), 1, file);

    size_t inode_bitmap_length = super_block.inodes_number / 8 + 1;
    size_t blocks_bitmap_length = super_block.blocks_number / 8 + 1;
    size_t inode_table_length = super_block.inode_size * super_block.inodes_number;
    size_t blocks_table_length = super_block.block_size * super_block.blocks_number;

    size_t total_length = inode_bitmap_length + blocks_bitmap_length + inode_table_length + blocks_table_length;

    char* bytes = malloc(total_length);
    memset(bytes, 0, total_length);
    fwrite(bytes, total_length, 1, file);
    free(bytes);

    if (init_empty_dir(file) != 0) return WRITE_FAILURE;
    return 0;
}

int open_fs(struct FS* fs) {

}

int close_fs(struct FS* fs) {

}

void dump_super_block(struct SuperBlock* super_block) {
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

int dump_bitmaps(FILE* file) {
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

void readline(char* buffer, size_t n) {
    fgets(buffer, n, stdin);
    buffer[strlen(buffer) - 1] = '\0';
}

void handle_error(int res) {
    switch (res) {
        case NO_SPACE:
            printf("No space left, file may be too big\n");
            break;
        case READ_FAILURE:
            printf("Read failure\n");
            break;
        case WRITE_FAILURE:
            printf("Write failure, file system may be corrupted\n");
            break;
        case TOO_SMALL_BUFFER:
            printf("Technical error: too small buffer\n");
            break;
        case WRONG_FILE_TYPE:
            printf("Wrong file type\n");
            break;
        case NOT_FOUND:
            printf("File not found\n");
            break;
        case WRONG_INPUT:
            printf("Wrong input\n");
            break;
        default:
            break;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Program must take exactly 1 argument!");
        return 1;
    }

    FILE* fs;
    if( access( argv[1], F_OK ) != -1 ) {
        fs = fopen(argv[1], "rb+");
    } else {
        fs = fopen(argv[1], "wb+");
        init_fs(fs);
    }

    const size_t buf1_size = 1024;
    const size_t buf2_size = 4096;
    char buf1[buf1_size];
    char buf2[buf2_size];

    while (1) {
        printf("command> ");
        readline(buf1, buf1_size);
        if (strcmp(buf1, "list") == 0) {
            printf("[list] path> ");
            readline(buf1, buf1_size);
            handle_error(list_by_name(fs, buf1));
        } else if (strcmp(buf1, "add") == 0) {
            printf("[add] path> ");
            readline(buf1, buf1_size);
            printf("[add] content> ");
            readline(buf2, buf2_size);
            handle_error(add_file_by_name(fs, buf1, buf2, strlen(buf2) + 1));
        } else if (strcmp(buf1, "update") == 0) {
            printf("[update] path> ");
            readline(buf1, buf1_size);
            printf("[update] content> ");
            readline(buf2, buf2_size);
            handle_error(update_file_by_name(fs, buf1, buf2, strlen(buf2) + 1));
        } else if (strcmp(buf1, "remove") == 0) {
            printf("[remove] path> ");
            readline(buf1, buf1_size);
            handle_error(remove_by_name(fs, buf1));
        } else if (strcmp(buf1, "exit") == 0) {
            fclose(fs);
            return 0;
        } else {
            printf("Unknown command: %s\n", buf1);
        }
    }
}
