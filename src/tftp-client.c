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

char *host = NULL;
int server_port = TFTP_SERVER_PORT;
char *file = NULL;
char *dest_file = NULL;

void printError(char *error) {
    fprintf(stderr, "ERROR: %s\n", error);
    exit(EXIT_FAILURE);
}

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s -h <hostname> [-p port] [-f filepath] -t <dest_filepath>\n", argv[0]);
    exit(EXIT_FAILURE);
}

void handleArguments(int argc, char **argv) {
    //Check number of argument
    if (argc < 5 || argc > 9) {
        printUsage(argv);
    }

    char option;
    //Get values of arguments
    while ((option = getopt(argc, argv, "h:p:f:t:")) != -1) {
        switch (option) {
        case 'h':
            host = optarg;
            break;
        case 'p':
            server_port = atoi(optarg);
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

    if (host == NULL) printUsage(argv);
    if (dest_file == NULL) printUsage(argv);
}

int main(int argc, char **argv) {
    handleArguments(argc, argv);

    // Create UDP socket
    int sockfd = -1;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) printError("Creating socket");

    // Configure server address
    struct hostent *server = gethostbyname(host);
    if (server == NULL) printError("No such host");

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    struct sockaddr *address = (struct sockaddr *) &server_addr;

    socklen_t server_len = sizeof(server_addr);

    char buffer[BUFFER_SIZE];
    char block[BLOCK_NUMBER_SIZE];
    char opcode[OPCODE_SIZE];
    char data[BUFFER_SIZE - 4];
    char ack_buffer[ACK_PACKET_SIZE];
    int bytes_rx;
    int bytes_tx;
    int expected_block;

    if (file) {
        expected_block = 1;

        // Construct the RRQ (Read Request) packet
        char mode[] = "netascii"; // "octet"?, any letter can be any case!
        int rrq_packet_len = 2 + strlen(file) + 1 + strlen(mode) + 1; // TO DO: EXTENSIONS
        char rrq_packet[rrq_packet_len];
        bzero(&rrq_packet, sizeof(rrq_packet));
        rrq_packet[1] = RRQ_OPCODE;
        memcpy(&rrq_packet[2], file, strlen(file));
        memcpy(&rrq_packet[2 + strlen(file) + 1], mode, strlen(mode));

        // Send the RRQ (Read Request) packet
        bytes_tx = sendto(sockfd, rrq_packet, rrq_packet_len, 0, address, server_len);
        if (bytes_tx < 0) printError("RRQ sendto");

        do {
            // RECEIVE DATA
            bzero(buffer, BUFFER_SIZE);
            bytes_rx = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, address, &server_len);
            if (bytes_rx < 0) printError("recvfrom not succesful");

            // GET OPCODE AND BLOCK NUMBERS
            strncpy(opcode, &buffer[0], 2);
            strncpy(block, &buffer[2], 2);

            // CHECK OPCODE AND BLOCK NUMBER
            if (atoi(opcode) != DATA_OPCODE) printError("unexpected opcode");
            if (atoi(block) != expected_block) printError("unexpected block");

            // GET DATA
            strncpy(data, &buffer[4], bytes_rx - 4);

            // HANDLE DATA

            // CREATE ACK PACKET
            bzero(ack_buffer, 4);
            strncpy(&ack_buffer[0], opcode, 2);
            strncpy(&ack_buffer[2], block, 2);

            // SEND ACK PACKET
            bytes_tx = sendto(sockfd, ack_buffer, 4, 0, address, server_len);
            if (bytes_tx < 0) printError("sendto not succesful");
            
            expected_block++;
        } while(bytes_rx >= BUFFER_SIZE);
        // Receive DATA BLOCK and send ACK for each one, if size is less then 512 it is the last DATA BLOCK
    } else {
        expected_block = 0;

        // Construct the WRQ (Write Request) packet
        char mode[] = "netascii"; // "octet"?
        int wrq_packet_len = 2 + strlen(dest_file) + 1 + strlen(mode) + 1;
        char wrq_packet[wrq_packet_len];
        bzero(&wrq_packet, sizeof(wrq_packet));
        wrq_packet[1] = WRQ_OPCODE;
        memcpy(&wrq_packet[2], dest_file, strlen(dest_file));
        memcpy(&wrq_packet[2 + strlen(dest_file) + 1], mode, strlen(mode));
        
        // Send the WRQ (Write Request) packet
        bytes_tx = sendto(sockfd, wrq_packet, wrq_packet_len, 0, address, sizeof(address));
        if (bytes_tx < 0) printError("WRQ sendto");
        
        // RECEIVE DATA
        bzero(buffer, 4);
        bytes_rx = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, address, &server_len);
        if (bytes_rx < 0) printError("recvfrom not succesful");

        strncpy(opcode, &buffer[0], 2);
        strncpy(block, &buffer[2], 2);
    
        if (atoi(opcode) != DATA_OPCODE) printError("unexpected opcode");
        if (atoi(block) != expected_block) printError("unexpected block");

        int new_port = atoi(&buffer[4]);
        // Receive ACK0 with new port from server, use this port for uploading DATA BLOCKS, receive ACK for each one

        do {
            // CONSTRUCT BUFFER
            bzero(buffer, BUFFER_SIZE);


            // SEND DATA PACKET
            bytes_tx = sendto(sockfd, buffer, BUFFER_SIZE, 0, address, server_len);
            if (bytes_tx < 0) printError("sendto not succesful");

            // RECEIVE ACK PACKET
            bytes_rx = recvfrom(sockfd, ack_buffer, 4, 0, address, &server_len);
            if (bytes_rx < 0) printError("recvfrom not succesful");

            // CHECK ACK PACKET
            strncpy(opcode, &ack_buffer[0], 2);
            strncpy(block, &ack_buffer[2], 2);

            if (atoi(opcode) != ACK_OPCODE) printError("unexpected opcode");
            if (atoi(block) != expected_block) printError("unexpected block");            
            expected_block++;
        } while(bytes_tx >= BUFFER_SIZE);
    }
    return 0;
}