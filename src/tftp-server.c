#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <getopt.h>
#include <stdbool.h>

typedef struct {
    unsigned short block_num;
    char data[512];
    int data_len;
} TFTPDataBlock;

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
        perror("Error assigning address and port");
        close(server_socket);
        exit(1);
    }

    socklen_t client_len = sizeof(server_address);
}