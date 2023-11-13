/* tftp-client.c ********************************************************
 * Name: Michal
 * Surname: Ondrejka
 * Login: xondre15
 * **********************************************************************
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <stdbool.h>
#include "tftp-client.h"

int sockfd;
struct sockaddr_in server_addr, recv_addr;
socklen_t recv_len = sizeof(recv_addr);

FILE *file;

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

void printRqPacket(char *rq_opcode, char *src_ip, int src_port, char *filepath, char *mode, int blksize, int timeout) {
    char *opts = "{$OPTS}";
    fprintf(stderr, "%s %s:%d \"%s\" %s %s", rq_opcode, src_ip, src_port, filepath, mode, opts);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void printAckPacket(char *scr_ip, int src_port, int block_id, int blksize, int timeout) {
    char opts[] = "{$OPTS}";
    if (block_id == 0) {
        fprintf(stderr, "OACK %s:%d %s", scr_ip, src_port, opts);
    } else {
        fprintf(stderr, "ACK %s:%d %d", scr_ip, src_port, block_id);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

void printDataPacket(char *src_ip, int src_port, int dest_port, int block_id) {
    fprintf(stderr, "DATA %s:%d %d %d", src_ip, src_port, dest_port, block_id);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void printErrorPacket(char *src_ip, int src_port, int dest_port, int code, char *message) {
    fprintf(stderr, "ERROR %s:%d %d %d \"%s\"", src_ip, src_port, dest_port, code, message);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void printPacket(char *packet, int size) {
    printf("Packet size: %d\n", size);
    for (int i = 0; i < size; i++) {
        printf("%02x ", (unsigned char)packet[i]);
    }
    printf("\n");
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

void createUDPSocket(int *sockfd) {
    *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*sockfd < 0) printError("Creating socket");
}

void closeUDPSocket() {
    if (sockfd == -1) return;
    close(sockfd);
    sockfd = -1;
}

void configureServerAddress(char *host, int server_port) {
    struct hostent *host_info = gethostbyname(host);
    if (host_info == NULL) printError("No such host");

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    memcpy(&server_addr.sin_addr.s_addr, host_info->h_addr_list[0], host_info->h_length);
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

void handleErrorPacket(char *packet) {
    uint16_t errorCode;
    memcpy(&errorCode, &packet[2], 2);

    char *errMsg = &packet[4];
    printError(errMsg);
}

int sendRqPacket(uint16_t opcode, char *filename, char *mode, int blksize, int timeout) {
    opcode = htons(opcode);
    
    int rq_packet_len = 2 + strlen(filename) + 1 + strlen(mode) + 1; // TO DO: EXTENSIONS
    char rq_packet[rq_packet_len];
    bzero(&rq_packet, sizeof(rq_packet));

    memcpy(&rq_packet[0], &opcode, 2);
    memcpy(&rq_packet[2], filename, strlen(filename));
    memcpy(&rq_packet[2 + strlen(filename) + 1], mode, strlen(mode));

    int bytes_tx = sendto(sockfd, rq_packet, rq_packet_len, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("RQ sendto");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    if (getsockname(sockfd, (struct sockaddr *)&client_addr, &client_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    if (ntohs(opcode) == RRQ_OPCODE) {
        printRqPacket("RRQ", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), filename, mode, blksize, timeout);
    } else if (ntohs(opcode) == WRQ_OPCODE) {
        printRqPacket("WRQ", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), filename, mode, blksize, timeout);
    }

    return bytes_tx;
}

int sendDataPacket(uint16_t block, uint16_t blksize) {
    uint16_t opcode = DATA_OPCODE;

    opcode = htons(opcode);
    block = htons(block);

    char packet_buffer[DATA_PACKET_SIZE];
    bzero(packet_buffer, DATA_PACKET_SIZE);
    memcpy(&packet_buffer[0], &opcode, 2);
    memcpy(&packet_buffer[2], &block, 2);

    int bytes_read = fread(&packet_buffer[4], 1, DATA_PACKET_SIZE - 4, file); // Upload needs DATA_PACKET_SIZE of chars to send data packet!! <= FIX

    int bytes_tx = sendto(sockfd, packet_buffer, bytes_read + 4, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("sendto not successful");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    if (getsockname(sockfd, (struct sockaddr *)&client_addr, &client_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    printDataPacket(inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), ntohs(server_addr.sin_port), ntohs(block));

    return bytes_tx;
}

int receiveDataPacket(uint16_t expected_block, uint16_t blksize) {
    char packet_buffer[DATA_PACKET_SIZE];
    bzero(packet_buffer, sizeof(packet_buffer));
    
    int bytes_rx = recvfrom(sockfd, packet_buffer, DATA_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");    
    if (bytes_rx < 4) printError("too little bytes in receive data packet");

    uint16_t opcode;
    uint16_t block;

    memcpy(&opcode, &packet_buffer[0], 2);
    memcpy(&block, &packet_buffer[2], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    if (opcode == ERROR_OPCODE) handleErrorPacket(packet_buffer);
    else if (opcode != DATA_OPCODE) printError("unexpected opcode while receiving data");

    if (block != expected_block) printError("unexpected block while receiving data");

    char data[DATA_PACKET_SIZE - OPCODE_SIZE - BLOCK_NUMBER_SIZE + 1];
    bzero(data, sizeof(data));
    memcpy(data, &packet_buffer[4], bytes_rx - 4);
    if (fprintf(file, "%s", data) < 0) printError("appending to file");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    if (getsockname(sockfd, (struct sockaddr *)&client_addr, &client_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    char source_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(server_addr.sin_addr), source_ip, INET_ADDRSTRLEN);
    printDataPacket(source_ip, ntohs(server_addr.sin_port), ntohs(client_addr.sin_port), block);

    return bytes_rx;
}

int sendAckPacket(uint16_t block) {
    uint16_t opcode = ACK_OPCODE;

    opcode = htons(opcode);
    block = htons(block);

    char packet_buffer[ACK_PACKET_SIZE];
    bzero(packet_buffer, 4);

    memcpy(&packet_buffer[0], &opcode, 2);
    memcpy(&packet_buffer[2], &block, 2);

    int bytes_tx = sendto(sockfd, packet_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("sendto not succesful");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    if (getsockname(sockfd, (struct sockaddr *)&client_addr, &client_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    printAckPacket(inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), ntohs(block), -1, -1);

    return bytes_tx;
}

int receiveAckPacket(uint16_t expected_block) {
    char packet_buffer[ACK_PACKET_SIZE];
    bzero(packet_buffer, sizeof(packet_buffer));

    int bytes_rx = recvfrom(sockfd, packet_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    uint16_t opcode;
    uint16_t block;

    memcpy(&opcode, &packet_buffer[0], 2);
    memcpy(&block, &packet_buffer[2], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    if (opcode == ERROR_OPCODE) handleErrorPacket(packet_buffer);
    else if (opcode != ACK_OPCODE) printError("unexpected opcod while receiving ack");
    if (block != expected_block) printError("unexpected block while receiving ack");

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    if (getsockname(sockfd, (struct sockaddr *)&client_addr, &client_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    char source_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(server_addr.sin_addr), source_ip, INET_ADDRSTRLEN);
    printAckPacket(source_ip, ntohs(server_addr.sin_port), ntohs(expected_block), -1, -1);

    return bytes_rx;
}

int sendErrorPacket(int errorCode, char *errMsg) {
    uint16_t opcode = ERROR_OPCODE;

    opcode = htons(opcode);
    errorCode = htons(errorCode);

    int error_packet_len = 2 + 2 + strlen(errMsg) + 1;
    char error_buffer[error_packet_len];
    bzero(error_buffer, sizeof(error_buffer));

    memcpy(&error_buffer[0], &opcode, 2);
    memcpy(&error_buffer[2], &errorCode, 2);
    memcpy(&error_buffer[4], errMsg, strlen(errMsg));

    int bytes_tx = sendto(sockfd, error_buffer, error_packet_len, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("sendto not successful");

    return bytes_tx;
}

void handleTimeout(int timeout) {
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    int n = select(sockfd + 1, &fds, NULL, NULL, &tv);

    if (n < 0) {
        printError("select failed");
    } else if (n == 0) {
        printError("timed out");
    }
}

int main(int argc, char **argv) {
    char mode[] = "netascii";
    int blksize = DEFAULT_BLKSIZE;
    int timeout = DEFAULT_TIMEOUT;

    char *host = NULL;
    int server_port = TFTP_SERVER_PORT;
    char *filepath = NULL;
    char *dest_file = NULL;

    handleArguments(argc, argv, &host, &server_port, &filepath, &dest_file);

    createUDPSocket(&sockfd);

    configureServerAddress(host, server_port);

    uint16_t block;

    if (filepath) { //download
        block = 1;
        int bytes_rx = -1;

        sendRqPacket(RRQ_OPCODE, filepath, mode, blksize, timeout);

        openFile(mode, filepath, dest_file);

        do {
            handleTimeout(timeout);

            bytes_rx = receiveDataPacket(block, blksize);

            if (block == 1) {
                server_port = ntohs(recv_addr.sin_port);
                configureServerAddress(host, server_port);   
            }

            sendAckPacket(block);
            
            block++;
        } while(bytes_rx >= DATA_PACKET_SIZE);

    } else { // upload
        block = 0;
        int bytes_tx = -1;

        sendRqPacket(WRQ_OPCODE, dest_file, mode, blksize, timeout);
        
        handleTimeout(timeout);
        receiveAckPacket(block);

        server_port = ntohs(recv_addr.sin_port);
        configureServerAddress(host, server_port);

        openFile(mode, filepath, dest_file);

        do {
            bytes_tx = sendDataPacket(block, blksize);

            handleTimeout(timeout);
            receiveAckPacket(block);

            block++;    
        } while(bytes_tx >= DATA_PACKET_SIZE);
    }

    closeUDPSocket();
    fclose(file);

    return 0;
}