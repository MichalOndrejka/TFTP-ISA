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
#define BUFFER_SIZE 512
#define ACK_PACKET_SIZE 4
#define OPCODE_SIZE 2
#define BLOCK_NUMBER_SIZE 2   
#define RRQ_OPCODE 1
#define WRQ_OPCODE 2
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5

int port = TFTP_SERVER_PORT;
char *root_dirpath = NULL;
FILE *file;

void printError(char *error) {
    fprintf(stderr, "ERROR: %s\n", error);
    exit(EXIT_FAILURE);
}

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s [-p port] -f root_dirpath\n", argv[0]);
    exit(EXIT_FAILURE);
}

void handleArgument(int argc, char **argv) {
    //Check number of argument
    if (argc < 5 || argc > 9) {
        printUsage(argv);
    }

    char option;
    //Get values of arguments
    while ((option = getopt(argc, argv, "p:f:")) != -1) {
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

int main(int argc, char **argv) {
    handleArgument(argc, argv);

    //CREATE UDP SOCKET
    int server_socket = -1;
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket <= 0) printError("socket");

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        printError("bind error");
    }

    if ((listen(server_socket, 10)) < 0) {
        perror("listen");
    }

    char buffer[BUFFER_SIZE];
    int16_t block;
    int16_t opcode;
    char data[BUFFER_SIZE - 4];
    char ack_buffer[ACK_PACKET_SIZE];
    int bytes_rx;
    int bytes_tx;
    int16_t expected_block;

    struct sockaddr *client_addr;
    socklen_t  client_len = sizeof(client_addr);

    while(true) {
        int new_fd = accept(server_socket, (struct sockaddr *) client_addr, &client_len);
        if (new_fd < 0) printError("accept");

        pid_t pid = fork();
        if (pid != 0) { // PARENT PROCESS
            close(new_fd);
            continue;
        } else { // CHILD PROCESS
            close(server_socket);
            // RECEIVE DATA
            bzero(buffer, BUFFER_SIZE);
            bytes_rx = recvfrom(new_fd, buffer, BUFFER_SIZE, 0, client_addr, &client_len);
            if (bytes_rx < 0) printError("recvfrom not succesful");

            memcpy(&opcode, &buffer[0], 2); // GET OPCODE
            char filename[512];
            strcpy(filename, &buffer[2]); // GET FILENAME
            char mode[512];
            strcpy(mode, &buffer[2 + strlen(filename) + 1]); // GET MODE

            if (opcode == RRQ_OPCODE) {
                if (strcmp(mode, "netascii")) {
                    file = fopen(filename, "r");
                } else if (strcmp(mode, "octet"))
                {
                    file = fopen(filename, "rb");
                }
                if (file == NULL) printError("opening file for read");

                expected_block = 1;
                opcode = DATA_OPCODE;
                int bytes_read = -1;
                do {
                    // SET DATA PACKET
                    bytes_read = fread(&buffer[4], BUFFER_SIZE - 4, BUFFER_SIZE - 4, file);

                    block = expected_block;
                    memcpy(&buffer[0], &opcode, 2);
                    memcpy(&buffer[2], &block, 2);

                    // Send the DATA packet
                    bytes_tx = sendto(new_fd, buffer, 4 + bytes_read, 0, client_addr, client_len);
                    if (bytes_tx < 0) printError("DATA sendto");

                    // Receive ACK
                    bzero(ack_buffer, 4);
                    bytes_rx = recvfrom(new_fd, ack_buffer, ACK_PACKET_SIZE, 0, client_addr, &client_len);
                    if (bytes_rx < 0) printError("DATA recvfrom");

                    // CHECK ACK PACKET
                    memcpy(&opcode, &ack_buffer[0], 2);
                    memcpy(&block, &ack_buffer[2], 2);

                    if (opcode != ACK_OPCODE) printError("unexpected opcode");
                    if (block  != expected_block) printError("unexpected block"); 

                    expected_block++;
                } while (bytes_read >= BUFFER_SIZE - 4);
                
            } else if (opcode == WRQ_OPCODE)
            {
                file = fopen(filename, "w");
                if (file == NULL) printError("creating file");
                fclose(file);


            } else printError("Unexpected opcode");
            


            break;
        }
    }
}