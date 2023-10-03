#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <getopt.h>
#include <stdbool.h>

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s -h hostname [-p port] [-f filepath] -t dest_filepath\n", argv[0]);
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
    while ((option = getopt(argc, argv, "h:p::f::t:")) != -1) {
        switch (option) {
        case 'h':
            host = optarg;
            break;
        case 'p':
            port = atoi(optarg);
            break;
        case 'f':
            file = optarg;
            break;
        case 't':
            dest_file = optarg;
            break;    
        default:
            printUsage(argv);
            break;
        }
    }
}