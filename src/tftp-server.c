#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <getopt.h>
#include <stdbool.h>

#define TFTP_SERVER_PORT 69
#define MAX_BUFFER_SIZE 512
#define RRQ_OPCODE 1
#define WRQ_OPCODE 2
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5

void printError(char *error) {
    fprintf(stderr, "ERROR: %s\n", error);
    exit(EXIT_FAILURE);
}

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s [-p port] -f root_dirpath\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    //Check number of argument
    if (argc < 5 || argc > 9) {
        printUsage(argv);
    }

    char option;
    int port = TFTP_SERVER_PORT;
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

    //CREATE UDP SOCKET
    int server_socket = -1;
    int family = AF_INET;
    int type = SOCK_DGRAM;

    server_socket = socket(family, type, 0);
    if (server_socket <= 0) {
        printError("socket");
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = family;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        printError("bind error");
    }

    if ((listen(server_socket, 10)) < 0) {
        printError("listen error");
    }

    while(true) {
        struct sockaddr_storage client_addr;
        socklen_t  sin_size = sizeof client_addr;
        int new_fd = accept(server_socket, (struct sockaddr *)&client_addr, &sin_size);
        if (new_fd == -1) {
            printError("accept");
            continue;
        }
    }
}