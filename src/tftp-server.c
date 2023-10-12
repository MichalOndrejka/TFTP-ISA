#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <getopt.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdarg.h>
#include <assert.h>
#include <fcntl.h>

typedef struct {
    unsigned short block_num;
    char data[512];
    int data_len;
} TFTPDataBlock;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s [-p port] root_dirpath\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    //Check number of argument
    if (argc < 5 || argc > 9) {
        printUsage(argv);
    }

    char option;
    int port = 69;
    char *root_dirpath = NULL;

    //Get values of arguments
    while ((option = getopt(argc, argv, "p::f:")) != -1) {
        switch (option) {
        case 'p':
            port = atoi(optarg);
            break;
        case 'f':
            root_dirpath = optarg;
            break;  
        default:
            printUsage(argv);
            break;
        }
    }

    printf("port: %d, root_dirpath = %s", port, root_dirpath);

    //CREATE UDP SOCKET
    int server_socket = -1;
    int family = AF_INET;
    int type = SOCK_DGRAM;

    server_socket = socket(family, type, 0);
    if (server_socket <= 0) {
        fprintf(stderr, "ERROR: socket\n");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = family;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "ERROR: bind error\n");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if ((listen(server_socket, 10)) < 0) {
        fprintf(stderr, "ERROR: listen error\n");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    while(true) {
        struct sockaddr_storage client_addr;
        socklen_t  sin_size = sizeof client_addr;
        int new_fd = accept(server_socket, (struct sockaddr *)&client_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }

        char s[INET6_ADDRSTRLEN];
        inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s, sizeof s);
        printf("server: got connection from %s\n", s);
    }

    socklen_t client_len = sizeof(server_address);
}