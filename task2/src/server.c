#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>

#include "../../task1/src/fs.h"

int server_fd, client_fd;

struct FS fs;

void termination_handler(int signum) {
    printf("Shutting down: %d\n", signum);
    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
    close(server_fd);
    fs_close(&fs);
    exit(1);
}

void handle_error(int client_fd, int res) {
    switch (res) {
        case NO_SPACE:
            send(client_fd, "No space left, file may be too big\n", 36, 0);
            break;
        case READ_FAILURE:
            send(client_fd, "Read failure\n", 14, 0);
            break;
        case WRITE_FAILURE:
            send(client_fd, "Write failure, file system may be corrupted\n", 45, 0);
            break;
        case TOO_SMALL_BUFFER:
            send(client_fd, "Technical error: too small buffer\n", 35, 0);
            break;
        case WRONG_FILE_TYPE:
            send(client_fd, "Wrong file type\n", 17, 0);
            break;
        case NOT_FOUND:
            send(client_fd, "File not found\n", 16, 0);
            break;
        case WRONG_INPUT:
            send(client_fd, "Wrong input\n", 13, 0);
            break;
        default:
            break;
    }
}

int main (int argc, char *argv[]) {
    if (argc < 2) {
        printf("Use: %s [filename] [port]\n", argv[0]);
        return 1;
    }

    pid_t pid;

    pid = fork();

    if (pid < 0) return 1;
    if (pid > 0) return 0;
    if (setsid() < 0) return 1;

    signal(SIGINT, termination_handler);
    signal(SIGTERM, termination_handler);

    pid = fork();

    if (pid < 0) return 1;
    if (pid > 0) return 0;

    umask(0);

    fs_open(&fs, argv[1]);

    int port = atoi(argv[2]);

    int res;
    struct sockaddr_in server, client;
    const size_t buffer_size = 8192;
    char buffer[buffer_size];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("Could not create socket\n");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    res = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
    if (res < 0) {
        printf("Could not bind socket\n");
        return 1;
    }

    res = listen(server_fd, 128);
    if (res < 0) {
        printf("Could not listen on socket\n");
        return 1;
    }

    printf("Server is listening on %d\n", port);

    while (1) {
        socklen_t client_len = sizeof(client);
        client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);

        if (client_fd < 0) {
            printf("Could not establish new connection\n");
            continue;
        }

        size_t bytes_read = 0;

        while (1) {
            bytes_read = recv(client_fd, buffer, buffer_size, 0);
            if (bytes_read >= buffer_size) {
                send(client_fd, "Message is too long!\n", 22, 0);
                close(client_fd);
                break;
            }
            if (!bytes_read) break;
            if (bytes_read < 0) {
                printf("Client read failed\n");
                continue;
            }

            if (buffer[bytes_read - 2] == '\r') {
                buffer[bytes_read - 2] = 0;
                bytes_read -= 2;
            }

            if (strcmp(buffer, "shutdown") == 0) {
                send(client_fd, "Exiting...\n", 12, 0);
                shutdown(client_fd, SHUT_RDWR);
                close(client_fd);
                close(server_fd);
                fs_close(&fs);
                return 0;
            } else if (strcmp(buffer, "help") == 0) {
                send(client_fd, "add <path> <content> - add file\nadd <path> - add dir\nread <path> - print file or dir\nupdate <path> <content> - update file\nremove <path> - remove file or dir (recursively)\nshutdown - shutdown server\n/a/b/c/ - example path to dir\n/a/b/c - example path to file\n", 260, 0);
                continue;
            }

            size_t first_space;
            for (first_space = 0; buffer[first_space] != ' ' && first_space < bytes_read; first_space++);
            size_t second_space;
            for (second_space = first_space + 1; buffer[second_space] != ' ' && second_space < bytes_read; second_space++);

            if (first_space == 0) {
                send(client_fd, "Unknown command\n", 30, 0);
                continue;
            }

            if (second_space == 0) second_space = bytes_read;

            char* command = malloc(first_space + 1);
            memcpy(command, buffer, first_space);
            command[first_space] = 0;

            char* path = malloc(second_space - first_space + 1);
            memcpy(path, buffer + first_space + 1, second_space - first_space - 1);
            path[second_space - first_space - 1] = 0;

            if (strcmp(command, "read") == 0) {
                int size = fs_size(&fs, path);
                if (size >= 0) {
                    char* content = malloc(size);
                    handle_error(client_fd, fs_read(&fs, path, content, size));
                    send(client_fd, content, size, 0);
                    free(content);
                } else {
                    handle_error(client_fd, size);
                }
            } else if (strcmp(command, "add") == 0) {
                res = fs_add(&fs, path, buffer + second_space + 1, strlen(buffer + second_space + 1) + 1);
                if (res < 0) {
                    handle_error(client_fd, res);
                } else {
                    send(client_fd, "OK\n", 4, 0);
                }
            } else if (strcmp(command, "update") == 0) {
                res = fs_update(&fs, path, buffer + second_space + 1, strlen(buffer + second_space + 1) + 1);
                if (res < 0) {
                    handle_error(client_fd, res);
                } else {
                    send(client_fd, "OK\n", 4, 0);
                }
            } else if (strcmp(command, "remove") == 0) {
                res = fs_remove(&fs, path);
                if (res < 0) {
                    handle_error(client_fd, res);
                } else {
                    send(client_fd, "OK\n", 4, 0);
                }
            } else {
                send(client_fd, "Unknown command\n", 17, 0);
            }

            free(path);
            free(command);
        }
    }

    return 0;
}