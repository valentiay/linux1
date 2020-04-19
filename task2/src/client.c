#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Use: %s [address] [port]\n", argv[0]);
        return 1;
    }

    in_addr_t addr = inet_addr(argv[1]);
    if (addr == -1) {
        printf("Invalid address: %s\n", argv[1]);
        return 1;
    }

    int port = atoi(argv[2]);
    if (port == 0) {
        printf("Invalid port: %s\n", argv[2]);
        return 1;
    }

    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("Failed to create socket\n");
        return 1;
    }

    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = addr;
    servaddr.sin_port = htons(port);

    if (connect(sockfd, (struct sockaddr * ) & servaddr, sizeof(servaddr)) != 0) {
        printf("Failed to connect to server\n");
        return 1;
    }

    size_t buffer_size = 8192;
    char buffer[buffer_size];
    while(1) {
        printf("command> ");
        fgets(buffer, buffer_size, stdin);
        size_t bytes_read = strlen(buffer);
        buffer[bytes_read - 1] = '\0';

        if ((strcmp(buffer, "exit")) == 0) {
            printf("Disconnecting...\n");
            break;
        }

        write(sockfd, buffer, bytes_read);
        read(sockfd, buffer, buffer_size);
        printf("%s", buffer);
    }

    close(sockfd);
}