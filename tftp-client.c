#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <getopt.h>
#include <stdbool.h>

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s -h <hostname> [-p port] [-f filepath] -t <dest_filepath>\n", argv[0]);
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    //Check number of argument
    if (argc < 5 || argc > 9) {
        printUsage(argv);
    }

    char option;
    char *host = NULL;
    int port = 69;
    char *file = NULL;
    char *dest_file = NULL;

    //Get values of arguments
    while ((option = getopt(argc, argv, "h:p:f:t:")) != -1) {
        switch (option) {
        case 'f':
            file = optarg;
            break;
        case 'h':
            host = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 't':
            dest_file = optarg;
            break;    
        default:
            printUsage(argv);
            break;
        }
    }

    if (host == NULL) printUsage(argv);
    if (dest_file == NULL) printUsage(argv);

    printf("host = %s, port = %d, file = %s, dest_file = %s\n", host, port, file, dest_file);

    int type = SOCK_DGRAM;

    //CREATE SOCKET
    int family = AF_INET;
    int client_socket = -1;

    client_socket = socket(family, type, 0);
    if (client_socket <= 0) {
        fprintf(stderr, "ERROR: socket\n");
        exit(EXIT_FAILURE);
    }

    //GET ADDRESS OF SERVER
    struct hostent *server = gethostbyname(host);
    if (server == NULL) {
        fprintf(stderr, "ERROR: no such host %s\n", host);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));

    server_address.sin_family = family;
    server_address.sin_port = htons(port);
    memcpy(&server_address.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    struct sockaddr *address = (struct sockaddr *) &server_address;
    socklen_t address_size = sizeof(server_address);
}