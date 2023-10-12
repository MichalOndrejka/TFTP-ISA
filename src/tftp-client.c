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

#define TFTP_SERVER_PORT 69
#define MAX_BUFFER_SIZE 512  // fixed TFTP length block size
#define RRQ_OPCODE 1
#define WRQ_OPCODE 2
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5

char *server_ip = NULL;
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

int main(int argc, char **argv) {
    //Check number of argument
    if (argc < 5 || argc > 9) {
        printUsage(argv);
    }

    char option;
    //Get values of arguments
    while ((option = getopt(argc, argv, "h:p:f:t:")) != -1) {
        switch (option) {
        case 'h':
            server_ip = optarg;
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

    if (server_ip == NULL) printUsage(argv);
    if (dest_file == NULL) printUsage(argv);

    // Create UDP socket
    int sockfd = -1;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) printError("Creating socket");

    // Configure server address
    struct hostent *server = gethostbyname(server_ip);
    if (server == NULL)
        printError("No such host");

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    struct sockaddr *address = (struct sockaddr *) &server_addr;

    socklen_t server_len = sizeof(server_addr);
    
    if (file) {
        // Construct the RRQ (Read Request) packet
        char mode[] = "netascii"; // "octet"?, any letter can be any case!
        int rrq_packet_len = 2 + strlen(file) + 1 + strlen(mode) + 1;
        char rrq_packet[rrq_packet_len];
        bzero(&rrq_packet, sizeof(rrq_packet));
        rrq_packet[1] = RRQ_OPCODE;
        memcpy(&rrq_packet[2], file, strlen(file));
        memcpy(&rrq_packet[2 + strlen(file) + 1], file, strlen(file));

        // Send the RRQ (Read Request) packet
        if (sendto(sockfd, rrq_packet, rrq_packet_len, 0, address, sizeof(address)) == -1)
            printError("ERROR: RRQ sendto");
        char buffer[MAX_BUFFER_SIZE];
        int bytes_rx;
        int bytes_tx;
        char block[2];
        char opcode[2];
        int expected_block = 1;
        char ack_buffer[4];
        char data[MAX_BUFFER_SIZE - 4];
        do {
            // RECEIVE DATA
            bzero(buffer, MAX_BUFFER_SIZE);
            bytes_rx = recvfrom(sockfd, buffer, MAX_BUFFER_SIZE, 0, address, &server_len);
            if (bytes_rx < 0) {
                printError("recvfrom not succesful");
            }

            // GET OPCODE AND BLOCK NUMBERS
            strncpy(opcode, &buffer[0], 2);
            strncpy(block, &buffer[2], 2);
            strncpy(data, &buffer[4], bytes_rx - 4);
            
            // CHECK OPCODE AND BLOCK NUMBER
            if (atoi(opcode) != DATA_OPCODE) printError("ERROR: unexpected opcode");
            if (atoi(block) != expected_block) printError("ERROR: unexpected block");

            // CREATE ACK PACKET
            bzero(ack_buffer, 4);
            strncpy(&ack_buffer[0], opcode, 2);
            strncpy(&ack_buffer[2], block, 2);

            // SEND ACK PACKET
            bytes_tx = sendto(sockfd, ack_buffer, 4, 0, address, server_len);
            if (bytes_tx < 0) {
                printError("sendto not succesful");
            }
            
            expected_block++;
        } while(bytes_rx >= MAX_BUFFER_SIZE);
        // Receive DATA BLOCK and send ACK for each one, if size is less then 512 it is the last DATA BLOCK
    } else {
        // Construct the WRQ (Write Request) packet
        char mode[] = "netascii"; // "octet"?
        int wrq_packet_len = 2 + strlen(dest_file) + 1 + strlen(mode) + 1;
        char wrq_packet[wrq_packet_len];
        bzero(&wrq_packet, sizeof(wrq_packet));
        wrq_packet[1] = WRQ_OPCODE;
        memcpy(&wrq_packet[2], dest_file, strlen(dest_file));
        memcpy(&wrq_packet[2 + strlen(dest_file) + 1], dest_file, strlen(dest_file));
        
        // Send the WRQ (Write Request) packet
        if (sendto(sockfd, wrq_packet, wrq_packet_len, 0, address, sizeof(address)) == -1)
            printError("ERROR: WRQ sendto");
        
        // Receive ACK0 with new port from server, use this port for uploading DATA BLOCKS, receive ACK for each one
    }

    return 0;
}