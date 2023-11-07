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
#define DATA_PACKET_SIZE 516
#define ACK_PACKET_SIZE 4
#define OPCODE_SIZE 2
#define BLOCK_NUMBER_SIZE 2   
#define RRQ_OPCODE 1
#define WRQ_OPCODE 2
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5

int sockfd = -1;
struct sockaddr_in server_addr, recv_addr;
socklen_t recv_len = sizeof(recv_addr);

FILE *file;

void closeUDPSocket() {
    if (sockfd == -1) return;
    close(sockfd);
    sockfd = -1;
}

void printError(char *error) {
    fprintf(stdout, "ERROR: %s\n", error);
    fflush(stdout);
    closeUDPSocket();
    exit(EXIT_FAILURE);
}

void printUsage(char **argv) {
    fprintf(stdout, "Usage: %s -h <hostname> [-p port] [-f filepath] -t <dest_filepath>\n", argv[0]);
    exit(EXIT_FAILURE);
}

void printInfo(char *opcode, int16_t block, char *mode, int server_port, char *filename) {
    char source_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server_addr.sin_addr, source_ip, INET_ADDRSTRLEN);
    int source_port = ntohs(server_addr.sin_port);
    int dest_port = server_port;
    int code = -1;
    char message[] = "MESSAGE";

    if (!strcmp(opcode, "RRQ") || !strcmp(opcode, "WRQ")) {
        printf("%s %s:%d \"%s\" %s", opcode, source_ip, source_port, filename, mode);
    } else if (!strcmp(opcode, "ACK")) {
        printf("%s %s:%d %d", opcode, source_ip, source_port, block);
    } else if (!strcmp(opcode, "OACK")) {
        printf("%s %s:%d %d", opcode, source_ip, source_port, block);
    } else if (!strcmp(opcode, "DATA")) {
        printf("%s %s:%d:%d %d", opcode, source_ip, source_port, dest_port, block);
    } else if (!strcmp(opcode, "ERROR")) {
        printf("%s %s:%d:%d %d \"%s\"", opcode, source_ip, source_port, dest_port, code, message);
    }
    printf("\n");
    fflush(stdout);
}

void handleArguments(int argc, char **argv, char **host, int *server_port, char **filepath, char **dest_file) {
    if (argc < 5 || argc > 9) {
        printUsage(argv);
    }

    char option;
    while ((option = getopt(argc, argv, "h:p:f:t:")) != -1) {
        switch (option) {
        case 'h':
            *host = optarg;
            break;
        case 'p':
            *server_port = atoi(optarg);
            break;
        case 'f':
            *filepath = optarg;
            break;
        case 't':
            *dest_file = optarg;
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

void configureServerAddress(char *host, int server_port) {
    struct hostent *host_info = gethostbyname(host);
    if (host_info == NULL) printError("No such host");

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    memcpy(&server_addr.sin_addr.s_addr, host_info->h_addr_list[0], host_info->h_length);
}

void printPacket(char *packet, int size) {
    printf("Packet size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%02x ", (unsigned char)packet[i]);
    }
    printf("\n");
}

void sendRqPacket(int16_t opcode, char *mode, char *filename, int *bytes_tx) {
    opcode = htons(opcode);
    
    int rq_packet_len = 2 + strlen(filename) + 1 + strlen(mode) + 1; // TO DO: EXTENSIONS
    char rq_packet[rq_packet_len];
    bzero(&rq_packet, sizeof(rq_packet));
    memcpy(&rq_packet[0], &opcode, 2);
    memcpy(&rq_packet[2], filename, strlen(filename));
    memcpy(&rq_packet[2 + strlen(filename) + 1], mode, strlen(mode));

    *bytes_tx = sendto(sockfd, rq_packet, rq_packet_len, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (*bytes_tx < 0) printError("RQ sendto");

    opcode = ntohs(opcode);
}

void openFile(char *mode, char *filepath, char *dest_file) {
    if (filepath) {
        file = fopen(dest_file, "w");
        if (file == NULL) printError("creating file");
        fclose(file);

        if (!strcmp(mode, "netascii")) {
            file = fopen(dest_file,"a");
        } else if (!strcmp(mode, "octet")) {
            file = fopen(dest_file, "ab");
        }
        if (file == NULL) printError("opening file for append");
    } else {
        file = stdin;
        if (file == NULL) printError("opening file for read");
    }
}

void closeFile() {
    fclose(file);
}

void sendDataPacket(int16_t block, int *bytes_tx) {

    int16_t opcode = DATA_OPCODE;

    block = htons(block);
    opcode = htons(opcode);

    char data_buffer[DATA_PACKET_SIZE];
    bzero(data_buffer, DATA_PACKET_SIZE);
    memcpy(&data_buffer[0], &opcode, 2);
    memcpy(&data_buffer[2], &block, 2);

    int bytes_read = fread(&data_buffer[4], 1, DATA_PACKET_SIZE - 4, file);

    *bytes_tx = sendto(sockfd, data_buffer, bytes_read + 4, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (*bytes_tx < 0) printError("sendto not successful");
}

void receiveDataPacket(int16_t expected_block, int *bytes_rx) {
    int16_t expected_opcode = DATA_OPCODE;
    int16_t block;
    int16_t opcode;

    char data_buffer[DATA_PACKET_SIZE];
    bzero(data_buffer, DATA_PACKET_SIZE);
    *bytes_rx = recvfrom(sockfd, data_buffer, DATA_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, &recv_len);

    if (*bytes_rx < 0) printError("recvfrom not succesful");    

    memcpy(&opcode, &data_buffer[0], 2);
    opcode = ntohs(opcode);
    if (opcode != expected_opcode) printError("unexpected opcode while receiving data");

    memcpy(&block, &data_buffer[2], 2);
    block = ntohs(block);
    if (block != expected_block) printError("unexpected block while receiving data");

    char data[DATA_PACKET_SIZE - OPCODE_SIZE - BLOCK_NUMBER_SIZE + 1];
    bzero(data, sizeof(data));
    memcpy(data, &data_buffer[4], *bytes_rx - 4);

    if (fprintf(file, "%s", data) < 0) printError("appending to file");
}

void sendAckPacket(int16_t block, int *bytes_tx) {

    int16_t opcode = ACK_OPCODE;
    char ack_buffer[ACK_PACKET_SIZE];

    block = htons(block);
    opcode = htons(opcode);

    bzero(ack_buffer, 4);
    memcpy(&ack_buffer[0], &opcode, 2);
    memcpy(&ack_buffer[2], &block, 2);

    *bytes_tx = sendto(sockfd, ack_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (*bytes_tx < 0) printError("sendto not succesful");
}

void receiveAckPacket(int16_t expected_block, int *bytes_rx) {
    printf("Receiving ack\n");
    int16_t expected_opcode = ACK_OPCODE;
    int16_t block;
    int16_t opcode;
    char ack_buffer[ACK_PACKET_SIZE];

    *bytes_rx = recvfrom(sockfd, ack_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (*bytes_rx < 0) printError("recvfrom not succesful");

    memcpy(&opcode, &ack_buffer[0], 2);
    opcode = ntohs(opcode);
    if (opcode != ACK_OPCODE) printError("unexpected opcod while receiving ack");
    memcpy(&block, &ack_buffer[2], 2);
    block = ntohs(block);
    if (block != expected_block) printError("unexpected block while receiving ack");    
}

int main(int argc, char **argv) {
    char *host = NULL;
    int server_port = TFTP_SERVER_PORT;
    char *filepath = NULL;
    char *dest_file = NULL;

    int bytes_tx = -1;
    int bytes_rx = -1;

    char mode[] = "netascii";

    char filename[DATA_PACKET_SIZE - OPCODE_SIZE -2];

    handleArguments(argc, argv, &host, &server_port, &filepath, &dest_file);

    createUDPSocket(&sockfd);

    configureServerAddress(host, server_port);

    int16_t block;

    if (filepath) {
        strcpy(filename, dest_file);
        block = 1;

        sendRqPacket(RRQ_OPCODE, mode, filepath, &bytes_tx);
        printInfo("RRQ", -1, mode, server_port, filename);

        openFile(mode, filepath, dest_file);

        do {
            receiveDataPacket(block, &bytes_rx);
            printInfo("DATA", block, mode, server_port, filename);

            if (block == 1) {
                server_port = ntohs(recv_addr.sin_port);
                configureServerAddress(host, server_port);   
            }

            sendAckPacket(block, &bytes_tx);
            printInfo("ACK", block, mode, server_port, filename);
            
            block++;
        } while(bytes_rx >= DATA_PACKET_SIZE);

        closeFile();

    } else {
        strcpy(filename, dest_file);
        block = 0;

        sendRqPacket(WRQ_OPCODE, mode, dest_file, &bytes_tx);
        printInfo("WRQ", -1, mode, server_port, filename);
        
        receiveAckPacket(block, &bytes_rx);
        printInfo("ACK", block, mode, server_port, filename);

        server_port = ntohs(recv_addr.sin_port);
        configureServerAddress(host, server_port);

        openFile(mode, filepath, dest_file);


        do {
            sendDataPacket(block, &bytes_tx);
            printInfo("DATA", block, mode, server_port, filename);

            receiveAckPacket(block, &bytes_rx);
            printInfo("ACK", block, mode, server_port, filename);

            block++;    
        } while(bytes_tx >= DATA_PACKET_SIZE);
    }
    return 0;
}