#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fs.h"

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
    if (argc < 2) {
        printf("Use: %s [filename]\n", argv[0]);
        return 1;
    }

    struct FS fs;
    fs_open(&fs, argv[1]);

    const size_t buffer_size = 8192;
    char buffer[buffer_size];

    while (1) {
        printf("command> ");
        fgets(buffer, buffer_size, stdin);
        size_t bytes_read = strlen(buffer);
        buffer[bytes_read - 1] = '\0';

        if (strcmp(buffer, "exit") == 0) {
            fs_close(&fs);
            return 0;
        } else if (strcmp(buffer, "help") == 0) {
            printf("add <path> <content> - add file\nadd <path> - add dir\nread <path> - print file or dir\nupdate <path> <content> - update file\nremove <path> - remove file or dir (recursively)\nexit - leave\n/a/b/c/ - example path to dir\n/a/b/c - example path to file\n");
            continue;
        }

        size_t first_space;
        for (first_space = 0; buffer[first_space] != ' ' && first_space < bytes_read; first_space++);
        size_t second_space;
        for (second_space = first_space + 1; buffer[second_space] != ' ' && second_space < bytes_read; second_space++);

        if (first_space == 0) {
            printf("Unknown command: '%s'\n", buffer);
            continue;
        }
        char *command = malloc(first_space + 1);
        memcpy(command, buffer, first_space);
        command[first_space] = 0;

        char *path = malloc(second_space - first_space + 1);
        memcpy(path, buffer + first_space + 1, second_space - first_space - 1);
        path[second_space - first_space - 1] = 0;

        if (strcmp(command, "read") == 0) {
            int size = fs_size(&fs, path);
            if (size >= 0) {
                char *content = malloc(size);
                handle_error(fs_read(&fs, path, content, size));
                printf("%s\n", content);
                free(content);
            } else {
                handle_error(size);
            }
        } else if (strcmp(command, "add") == 0) {
            handle_error(fs_add(&fs, path, buffer + second_space + 1, strlen(buffer + second_space + 1) + 1));
        } else if (strcmp(command, "update") == 0) {
            handle_error(fs_update(&fs, path, buffer + second_space + 1, strlen(buffer + second_space + 1) + 1));
        } else if (strcmp(command, "remove") == 0) {
            handle_error(fs_remove(&fs, path));
        } else {
            printf("Unknown command: '%s'\n", command);
        }

        free(path);
        free(command);
    }
}
