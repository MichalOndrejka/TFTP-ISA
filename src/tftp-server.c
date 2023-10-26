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

int blksize = 0;
int timeout = 0;

int server_port = TFTP_SERVER_PORT;
in_addr_t in_addr = INADDR_ANY;
char *root_dirpath = NULL;

int server_socket = -1;
int sockfd = -1;
struct sockaddr_in client_addr, server_addr;
socklen_t client_len = sizeof(client_addr);

size_t bytes_rx;
size_t bytes_tx;
size_t bytes_read;

char data_buffer[DATA_PACKET_SIZE];
char data[DATA_PACKET_SIZE - OPCODE_SIZE - BLOCK_NUMBER_SIZE];
char mode[20];

bool send_file;

char filename[512 - OPCODE_SIZE];
char filepath[1024] = "";
FILE *file;

void printError(char *error) {
    fprintf(stderr, "ERROR: %s\n", error);
    exit(EXIT_FAILURE);
}

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s [-p port] -f root_dirpath\n", argv[0]);
    exit(EXIT_FAILURE);
}

void printInfo(char *opcode, int16_t block) {
    char source_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), source_ip, INET_ADDRSTRLEN);
    int source_port = ntohs(client_addr.sin_port);
    int dest_port = -1;
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

void handleArguments(int argc, char **argv) {
    if (argc < 3 || argc > 5) {
        printUsage(argv);
    }

    char option;
    while ((option = getopt(argc, argv, "p:f:")) != -1) {
        switch (option) {
        case 'p':
            server_port = atoi(optarg);
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

void closeUDPSocket(int *udp_socket) {
    if (*udp_socket == -1) return;
    close(*udp_socket);
    *udp_socket = -1;
}

void createUDPSocket(int *udp_socket) {
    *udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (*udp_socket < 0) printError("Creating socket");
}

void configureServerAddress() {
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = in_addr;
    server_addr.sin_port = htons(server_port);
}

void handleOptions(char *rq_packet) {
    char blcksize_opt[] = "blcksize";
    int min_blcksize = 8;
    int max_blcksize = 65464;

    char timeout_opt[] = "timeout";
    int min_timeout = 1;
    int max_timeout = 255;

    int filename_len = strlen(&rq_packet[2]);
    int mode_len = strlen(&rq_packet[2 + filename_len + 1]);

    int bytes_processed = 2 + filename_len + 1 + mode_len + 1;

    char *option;
    int value;

    while (bytes_processed < (bytes_rx)) {
        option = &rq_packet[bytes_processed];
        bytes_processed += strlen(option) + 1;

        if (strcmp(option, blcksize_opt)) {
            blksize = atoi(&rq_packet[bytes_processed]); //ntohs() ??
            if (blksize < min_blcksize || blksize > max_blcksize) {
                printError("invalid value for blksize option");
            }
        } else if (strcmp(option, timeout_opt)) {
            timeout = atoi(&rq_packet[bytes_processed]); //ntohs() ??
            if (timeout < min_timeout || timeout > max_timeout) {
                printError("invalid value for timeout option");
            }
        } else {
            printError("invalid option in rq packet");
        }
        bytes_processed += strlen(&rq_packet[bytes_processed]) + 1;
    }
    
}

void receiveRqPacket() {
    int16_t opcode;

    char rq_packet[DATA_PACKET_SIZE];
    bzero(rq_packet, DATA_PACKET_SIZE);

    bytes_rx = recvfrom(server_socket, rq_packet, DATA_PACKET_SIZE, 0, (struct sockaddr *) &client_addr, &client_len);
    if (bytes_rx < 0) perror("recvfrom not succesful");

    memcpy(&opcode, &rq_packet[0], 2);

    opcode = ntohs(opcode);

    if (opcode == RRQ_OPCODE) send_file = true;
    else if (opcode == WRQ_OPCODE) send_file = false;
    else printError("unexpected opcode while receive eq packet");

    strcpy(filename, &rq_packet[2]);

    strcpy(mode, &rq_packet[2 + strlen(filename) + 1]);

    if (OPCODE_SIZE + strlen(filename) + 1 + strlen(mode) + 1 < bytes_rx) {
        handleOptions(rq_packet);
    }
}

void openFile() {
    strcat(filepath, root_dirpath); // Could be problem here
    strcat(filepath, filename);

    if (send_file) {
        if (!strcmp(mode, "netascii")) {
            file = fopen(filepath, "r");
        } else if (!strcmp(mode, "octet")) {
            file = fopen(filepath, "rb");
        }
        if (file == NULL) printError("opening file for read");
    } else {
        file = fopen(filepath, "w");
        if (file == NULL) printError("creating file");
        fclose(file);

        if (!strcmp(mode, "netascii")) {
            file = fopen(filepath, "a");
        } else if (!strcmp(mode, "netascii")) {
            file = fopen(filepath, "ab");
        }
        if (file == NULL) printError("opening file for append");
    }
}

void closeFile() {
    fclose(file);
}

void sendDataPacket(int16_t block) {
    printInfo("DATA", block);

    int16_t opcode = DATA_OPCODE;
    
    bzero(data_buffer, DATA_PACKET_SIZE);

    block = htons(block);
    opcode = htons(opcode);

    memcpy(&data_buffer[0], &opcode, 2);
    memcpy(&data_buffer[2], &block, 2);

    bytes_read = fread(&data_buffer[4], 1, DATA_PACKET_SIZE - 4, file);

    bytes_tx = sendto(sockfd, data_buffer, 4 + bytes_read, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    if (bytes_tx < 0) printError("sendto not successful");
}

void receiveDataPacket(int16_t expected_block) {
    int16_t expected_opcode = DATA_OPCODE;
    int16_t block;
    int16_t opcode;

    bzero(data_buffer, DATA_PACKET_SIZE);
    bytes_rx = recvfrom(sockfd, data_buffer, DATA_PACKET_SIZE, 0, (struct sockaddr *) &client_addr, &client_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    memcpy(&opcode, &data_buffer[0], 2);
    opcode = ntohs(opcode);
    if (opcode != expected_opcode) printError("unexpected opcode while receive data");
    memcpy(&block, &data_buffer[2], 2);
    block = ntohs(block);
    if (block != expected_block) printError("unexpected block while receive data");

    memcpy(data, &data_buffer[4], bytes_rx - 4);

    if (fprintf(file, "%s", data) < 0) printError("appending to file");
}

void sendAckPacket(int16_t block) {
    printInfo("ACK", block);
    int16_t opcode = ACK_OPCODE;
    char ack_buffer[ACK_PACKET_SIZE];

    block = htons(block);
    opcode = htons(opcode);

    bzero(ack_buffer, 4);
    memcpy(&ack_buffer[0], &opcode, 2);
    memcpy(&ack_buffer[2], &block, 2);

    bytes_tx = sendto(sockfd, ack_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &client_addr, sizeof(client_addr));
    if (bytes_tx < 0) printError("sendto not succesful");
}

void receiveAckPacket(int16_t expected_block) {
    int16_t expected_opcode = ACK_OPCODE;
    int16_t block;
    int16_t opcode;
    char ack_buffer[ACK_PACKET_SIZE];

    bytes_rx = recvfrom(sockfd, ack_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &client_addr, &client_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    memcpy(&block, &ack_buffer[2], 2);
    memcpy(&opcode, &ack_buffer[0], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    if (opcode != ACK_OPCODE) printError("unexpected opcode while receive ack");
    if (block != expected_block) printError("unexpected block while receive ack");
}

int main(int argc, char **argv) {
    handleArguments(argc, argv);

    createUDPSocket(&server_socket);

    configureServerAddress();

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }

    while(true) {
        receiveRqPacket();

        pid_t pid = fork();
        if (pid != 0) {
            closeUDPSocket(&sockfd);
            continue;
        } else {
            closeUDPSocket(&server_socket);

            createUDPSocket(&sockfd);

            int16_t block;

            if (send_file) {
                openFile();

                block = 1;

                do {
                    sendDataPacket(block);

                    receiveAckPacket(block);

                    block++;
                } while (bytes_tx >= DATA_PACKET_SIZE);
                
            } else
            {
                openFile();

                block = 0;

                sendAckPacket(0);

                block++;

                do {
                    receiveDataPacket(block);

                    sendAckPacket(block);

                    block++;
                } while (bytes_rx >= DATA_PACKET_SIZE);
            }

            closeFile();

            closeUDPSocket(&sockfd);
            break;
        }
    }

    return 0;
}