#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <getopt.h>
#include <stdbool.h>

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
}