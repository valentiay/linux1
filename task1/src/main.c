#include <stdio.h>
#include <string.h>

#include "fs.h"

void readline(char *buffer, size_t n) {
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

    struct FS fs;
    fs_open(&fs, argv[1]);

    const size_t buf1_size = 1024;
    const size_t buf2_size = 4096;
    char buf1[buf1_size];
    char buf2[buf2_size];

    while (1) {
        printf("command> ");
        readline(buf1, buf1_size);
        if (strcmp(buf1, "read") == 0) {
            printf("[read] path> ");
            readline(buf1, buf1_size);
            handle_error(fs_read(&fs, buf1));
        } else if (strcmp(buf1, "add") == 0) {
            printf("[add] path> ");
            readline(buf1, buf1_size);
            printf("[add] content> ");
            readline(buf2, buf2_size);
            handle_error(fs_add(&fs, buf1, buf2, strlen(buf2) + 1));
        } else if (strcmp(buf1, "update") == 0) {
            printf("[update] path> ");
            readline(buf1, buf1_size);
            printf("[update] content> ");
            readline(buf2, buf2_size);
            handle_error(fs_update(&fs, buf1, buf2, strlen(buf2) + 1));
        } else if (strcmp(buf1, "remove") == 0) {
            printf("[remove] path> ");
            readline(buf1, buf1_size);
            handle_error(fs_remove(&fs, buf1));
        } else if (strcmp(buf1, "exit") == 0) {
            close_fs(&fs);
            return 0;
        } else {
            printf("Unknown command: %s\n", buf1);
        }
    }
}
