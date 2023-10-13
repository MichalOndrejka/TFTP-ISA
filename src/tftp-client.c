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
char *filepath = NULL;
char *dest_file = NULL;

int bytes_rx;
int bytes_tx;
int sockfd = -1
;struct sockaddr *address;
socklen_t address_len;
FILE *file;
char mode[] = "netascii";
char buffer[BUFFER_SIZE];
int16_t block;
int16_t opcode;
int16_t expected_block;
char data[BUFFER_SIZE - 4];
char ack_buffer[ACK_PACKET_SIZE];


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
            filepath = optarg;
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

void createUDPSocket() {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) printError("Creating socket");
}

void configureServerAddress() {
    struct hostent *server = gethostbyname(host);
    if (server == NULL) printError("No such host");

    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);

    address = (struct sockaddr *) &server_addr;
    address_len = sizeof(address);
    // server_addr.sin_port = htons(new_port);

}

void sendRqPacket(int16_t opcode) {
    char *filename;
    if (opcode == RRQ_OPCODE) filename = filepath;
    else if (opcode == WRQ_OPCODE) filename = dest_file;
    
    int rq_packet_len = 2 + strlen(filename) + 1 + strlen(mode) + 1; // TO DO: EXTENSIONS
    char rq_packet[rq_packet_len];
    bzero(&rq_packet, sizeof(rq_packet));
    memcpy(&rq_packet[0], &opcode, 2);
    memcpy(&rq_packet[2], filename, strlen(filename));
    memcpy(&rq_packet[2 + strlen(filename) + 1], mode, strlen(mode));

    // Send the RRQ/WRQ packet
    bytes_tx = sendto(sockfd, rq_packet, rq_packet_len, 0, address, address_len);
    if (bytes_tx < 0) printError("RQ sendto");
}

void openFile() {
    if (filepath) {
        file = fopen(dest_file, "w");
        if (file == NULL) printError("creating file");
        fclose(file);

        // APPEND ASCII OR BIN TO FILE
        if (strcmp(mode, "netascii")) {
            file = fopen(dest_file,"a");
        } else if (strcmp(mode, "octet")) {
            file = fopen(dest_file, "ab");
        }
        if (file == NULL) printError("opening file for append");
    } else {
        // READ ASCII OR BIN FROM FILE
        if (strcmp(mode, "netascii")) {
            file = fopen(filepath,"r");
        } else if (strcmp(mode, "octet")) {
            file = fopen(filepath, "rb");
        }
        if (file == NULL) printError("opening file for read");
    }
}

void closeFile() {
    fclose(file);
}

void sendDataPacket() {
    bzero(buffer, BUFFER_SIZE);
    memcpy(&buffer[0], &opcode, 2);
    memcpy(&buffer[2], &block, 2);
    int bytes_read = fread(&buffer[4], BUFFER_SIZE - 4, BUFFER_SIZE - 4, file);

    bytes_tx = sendto(sockfd, buffer, BUFFER_SIZE, 0, address, address_len);
    if (bytes_tx < 0) printError("sendto not succesful");
}

void receiveDataPacket(int expected_block) {
    // RECEIVE DATA
    bzero(buffer, BUFFER_SIZE);
    bytes_rx = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, address, &address_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    // GET OPCODE AND BLOCK NUMBERS
    memcpy(&opcode, &buffer[0], 2);
    memcpy(&block, &buffer[2], 2);

    // CHECK OPCODE AND BLOCK NUMBER
    if (opcode != DATA_OPCODE) printError("unexpected opcode");
    if (block != expected_block) printError("unexpected block");

    // GET DATA
    memcpy(data, &buffer[4], bytes_rx - 4);

    // HANDLE DATA
    if (fprintf(file, "%s", buffer) < 0) printError("appending to file");
}

void sendAckPacket() {
    // CREATE ACK PACKET
    bzero(ack_buffer, 4);
    memcpy(&ack_buffer[0], &opcode, 2);
    memcpy(&ack_buffer[2], &block, 2);

    // SEND ACK PACKET
    bytes_tx = sendto(sockfd, ack_buffer, 4, 0, address, address_len);
    if (bytes_tx < 0) printError("sendto not succesful");
}

void receiveAckPacket(int expected_block) {
    // RECEIVE ACK PACKET
    bytes_rx = recvfrom(sockfd, ack_buffer, 4, 0, address, &address_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    // CHECK ACK PACKET
    memcpy(&opcode, &ack_buffer[0], 2);
    memcpy(&block, &ack_buffer[2], 2);

    if (opcode != ACK_OPCODE) printError("unexpected opcode");
    if (block  != expected_block) printError("unexpected block");    
}

int main(int argc, char **argv) {
    handleArguments(argc, argv);

    createUDPSocket();

    configureServerAddress();

    if (filepath) {
        expected_block = 1;

        sendRqPacket(RRQ_OPCODE);

        openFile();

        do {
            receiveDataPacket(expected_block);

            opcode = ACK_OPCODE;
            sendAckPacket();
            
            expected_block++;
        } while(bytes_rx >= BUFFER_SIZE);

        closeFile();

    } else {
        expected_block = 0;

        sendRqPacket(WRQ_OPCODE);
        
        receiveDataPacket(expected_block);

        int new_port = atoi(&buffer[4]);

        opcode = DATA_OPCODE;
        block = 0;
        do {
            sendDataPacket();
            block++;
            expected_block++;

            receiveAckPacket(expected_block);        
        } while(bytes_tx >= BUFFER_SIZE);
    }
    return 0;
}