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
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <getopt.h>
#include <stdbool.h>
#include "tftp-client.h"

int sockfd;
struct sockaddr_in server_addr, recv_addr, src_addr;
socklen_t recv_len = sizeof(recv_addr);

FILE *file = NULL;

void printError(char *error, bool exit_failure) {
    fprintf(stdout, "Local error: %s\n", error);
    fflush(stdout);
    if (exit_failure) {
        if (file) fclose(file);
        closeUDPSocket();
        exit(EXIT_FAILURE);
    }
}

void printUsage(char **argv) {
    fprintf(stdout, "Usage: %s -h <hostname> [-p port] [-f filepath] -t <dest_filepath>\n", argv[0]);
    exit(EXIT_FAILURE);
}

void printRqPacket(char *rq_opcode, char *src_ip, int src_port, char *filepath, char *mode, char *blksize_val, char *timeout_val) {
    char opts[1024];
    bzero(opts, sizeof(opts));

    if (blksize_val) {
        if (atoi(blksize_val) != DEFAULT_BLKSIZE) {
            strcat(opts, "blksize=");
            strcat(opts, blksize_val);
            strcat(opts, " ");
        }
    }
    if (timeout_val) {
        if (atoi(timeout_val) != DEFAULT_TIMEOUT) {
            strcat(opts, "timeout=");
            strcat(opts, timeout_val);
            strcat(opts, " ");
        }
    }

    fprintf(stderr, "%s %s:%d \"%s\" %s %s", rq_opcode, src_ip, src_port, filepath, mode, opts);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void printAckPacket(char *scr_ip, int src_port, int block_id, int blksize, int timeout) {
    char opts[1024];
    bzero(opts, sizeof(opts));
    char blksize_val[1024];
    bzero(blksize_val, sizeof(blksize_val));
    sprintf(blksize_val, "%d", blksize);
    char timeout_val[1024];
    bzero(timeout_val, sizeof(timeout_val));
    sprintf(timeout_val, "%d", timeout);

    if (atoi(blksize_val) != DEFAULT_BLKSIZE) {
        strcat(opts, "blksize=");
        strcat(opts, blksize_val);
        strcat(opts, " ");
    }
    if (atoi(timeout_val) != DEFAULT_TIMEOUT) {
        strcat(opts, "timeout=");
        strcat(opts, timeout_val);
        strcat(opts, " ");
    }

    if (block_id == -1) {
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
        if (packet[i] <= 9) printf("%d ", packet[i]);
        else (printf("%c ", packet[i]));
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
    if (*sockfd < 0) printError("couldn't create scoket", true);
}

void closeUDPSocket() {
    if (sockfd == -1) return;
    close(sockfd);
    sockfd = -1;
}

void configureServerAddress(char *host, int server_port) {
    struct hostent *host_info = gethostbyname(host);
    if (host_info == NULL) printError("no such host", true);

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    memcpy(&server_addr.sin_addr.s_addr, host_info->h_addr_list[0], host_info->h_length);
}

void openFile(char *mode, char *filepath, char *dest_file) {
    if (filepath) {
        file = fopen(dest_file, "w");
        if (file == NULL) printError("creating file", true);
        fclose(file);

        if (!strcmp(mode, "netascii")) {
            file = fopen(dest_file,"a");
        } else if (!strcmp(mode, "octet")) {
            file = fopen(dest_file, "ab");
        }
        if (file == NULL) printError("opening file for append", true);
    }
}

void receiveOackPacket(int *blksize, int *timeout) {
    char packet_buffer[DEFAULT_BLKSIZE];
    bzero(packet_buffer, sizeof(packet_buffer));

    int bytes_rx = recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) perror("recvfrom not succesful");
    if (bytes_rx < 2) printError("too little bytes in receive data packet", true);

    char blksize_opt[] = "blksize";
    int blksize_val = DEFAULT_BLKSIZE;
    int min_blcksize = 8;
    int max_blcksize = 65464;

    char timeout_opt[] = "timeout";
    int timeout_val = DEFAULT_TIMEOUT;
    int min_timeout = 1;
    int max_timeout = 255;

    int bytes_processed = OPCODE_SIZE;

    char *option;
    char *value;

    while (bytes_processed < bytes_rx) {
        option = &packet_buffer[bytes_processed];
        bytes_processed += strlen(option) + 1;
        value = &packet_buffer[bytes_processed];
        bytes_processed += strlen(value) + 1;

        if (!strcmp(option, blksize_opt)) {
            blksize_val = atoi(value);
            if (*blksize < min_blcksize || *blksize > max_blcksize) {
                printError("invalid value for blksize option", true);
            }
        } else if (!strcmp(option, timeout_opt)) {
            timeout_val = atoi(value);
            if (*timeout < min_timeout || *timeout > max_timeout) {
                printError("invalid value for timeout option", true);
            }
        }
    }

    *blksize = blksize_val;
    *timeout = timeout_val;
}

int sendRqPacket(uint16_t opcode, char *filename, char *mode, int *blksize, int *timeout) {
    opcode = htons(opcode);

    int opts_len = 0;
    char blksize_opt[] = "blksize";
    char timeout_opt[] = "timeout";
    char blksize_val[64];
    bzero(blksize_val, sizeof(blksize_val));
    sprintf(blksize_val, "%d", *blksize);
    char timeout_val[64];
    bzero(timeout_val, sizeof(timeout_val));
    sprintf(timeout_val, "%d", *timeout);
    if (*blksize != DEFAULT_BLKSIZE) opts_len += strlen(blksize_opt) + 1 + strlen(blksize_val) + 1;
    if (*timeout != DEFAULT_TIMEOUT) opts_len += strlen(timeout_opt) + 1 + strlen(timeout_val) + 1;

    int packet_buffer_len = 2 + strlen(filename) + 1 + strlen(mode) + 1 + opts_len;
    char packet_buffer[packet_buffer_len];
    bzero(packet_buffer, sizeof(packet_buffer));

    int curr_byte = 0;
    memcpy(&packet_buffer[curr_byte], &opcode, 2);
    curr_byte += 2;
    memcpy(&packet_buffer[curr_byte], filename, strlen(filename));
    curr_byte += strlen(filename) + 1;
    memcpy(&packet_buffer[curr_byte], mode, strlen(mode));
    curr_byte += strlen(mode) + 1;

    if (*blksize != DEFAULT_BLKSIZE) {
        memcpy(&packet_buffer[curr_byte], &blksize_opt, strlen(blksize_opt));
        curr_byte += strlen(blksize_opt) + 1;
        memcpy(&packet_buffer[curr_byte], &blksize_val, strlen(blksize_val));
        curr_byte += strlen(blksize_val) + 1;
    }
    if (*timeout != DEFAULT_TIMEOUT) {
        memcpy(&packet_buffer[curr_byte], &timeout_opt, strlen(timeout_opt));
        curr_byte += strlen(timeout_opt) + 1;
        memcpy(&packet_buffer[curr_byte], &timeout_val, strlen(timeout_val));
        curr_byte += strlen(timeout_val) + 1;
    }

    int bytes_tx = sendto(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("rq packet sendto failed", true);

    if (opts_len > 0) receiveOackPacket(blksize, timeout);

    if (ntohs(opcode) == RRQ_OPCODE) {
        printRqPacket("RRQ", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), filename, mode, blksize_val, timeout_val);
    } else if (ntohs(opcode) == WRQ_OPCODE) {
        printRqPacket("WRQ", inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), filename, mode, blksize_val, timeout_val);
    }

    if (opts_len > 0) printAckPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), -1, *blksize, *timeout);

    return bytes_tx;
}

int sendErrorPacket(uint16_t error_code, char *error_msg) {
    uint16_t opcode = ERROR_OPCODE;

    opcode = htons(opcode);
    error_code = htons(error_code);

    int packet_buffer_len = OPCODE_SIZE + EEROR_CODE_SIZE + strlen(error_msg) + 1;
    char packet_buffer[packet_buffer_len];
    bzero(packet_buffer, sizeof(packet_buffer));
    memcpy(&packet_buffer[0], &opcode, 2);
    memcpy(&packet_buffer[2], &error_code, 2);

    memcpy(&packet_buffer[4], error_msg, strlen(error_msg));

    int bytes_tx = sendto(sockfd, packet_buffer, packet_buffer_len, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("sendto not successful", true);

    printErrorPacket(inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), ntohs(server_addr.sin_port), ntohs(error_code), error_msg);

    if (error_code == 5) printError(error_msg, false);
    else printError(error_msg, true);

    return bytes_tx;
}

void handleErrorPacket(char *packet) {
    uint16_t error_code;
    memcpy(&error_code, &packet[2], 2);
    error_code = ntohs(error_code);
    char *error_msg = &packet[4];

    printErrorPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), ntohs(src_addr.sin_port), error_code, error_msg);

    if (error_code == 5) printError(error_msg, false);
    else printError(error_msg, true);
}

int sendDataPacket(uint16_t block, uint16_t blksize, bool is_retransmit, char *stdin_data, int stdin_data_index) {
    uint16_t opcode = DATA_OPCODE;

    opcode = htons(opcode);
    block = htons(block);

    char packet_buffer[blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE];
    bzero(packet_buffer, sizeof(packet_buffer));
    memcpy(&packet_buffer[0], &opcode, 2);
    memcpy(&packet_buffer[2], &block, 2);

    int bytes_read = strlen(&stdin_data[stdin_data_index]);
    if (bytes_read > blksize) bytes_read = blksize;
    memcpy(&packet_buffer[4], &stdin_data[stdin_data_index], bytes_read);

    int bytes_tx = sendto(sockfd, packet_buffer, bytes_read + OPCODE_SIZE + BLOCK_NUMBER_SIZE, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("sendto not successful", true);

    printDataPacket(inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), ntohs(server_addr.sin_port), ntohs(block));

    return bytes_tx;
}

int receiveDataPacket(uint16_t expected_block, uint16_t blksize) {
    char packet_buffer[blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE];
    bzero(packet_buffer, sizeof(packet_buffer));
    
    int bytes_rx = recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("data packet recvfrom failed", true);    
    if (bytes_rx < 4) printError("too little bytes in data packet recvfrom", true);

    uint16_t opcode;
    uint16_t block;

    memcpy(&opcode, &packet_buffer[0], 2);
    memcpy(&block, &packet_buffer[2], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    if (opcode == ERROR_OPCODE) handleErrorPacket(packet_buffer);
    else if (opcode != DATA_OPCODE) sendErrorPacket(4, "Illegal TFTP operation, unexpected opcode");

    if (block != expected_block) sendErrorPacket(4, "Illegal TFTP operation, unexpected block");

    char data[blksize + 1];
    bzero(data, sizeof(data));
    memcpy(data, &packet_buffer[4], bytes_rx - 4);
    if (fprintf(file, "%s", data) < 0) sendErrorPacket(3, "Disk full or allocation exceeded");

    printDataPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), ntohs(src_addr.sin_port), block);

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
    if (bytes_tx < 0) printError("ack packet sendto failed", true);

    printAckPacket(inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), ntohs(block), -1, -1);

    return bytes_tx;
}

int receiveAckPacket(uint16_t expected_block) {
    char packet_buffer[ACK_PACKET_SIZE];
    bzero(packet_buffer, sizeof(packet_buffer));

    int bytes_rx = recvfrom(sockfd, packet_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("recvfrom not succesful", true);
    if (bytes_rx < 4) printError("too little bytes in ack packet recvfrom", true);

    uint16_t opcode;
    uint16_t block;

    memcpy(&opcode, &packet_buffer[0], 2);
    memcpy(&block, &packet_buffer[2], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    if (opcode == ERROR_OPCODE) handleErrorPacket(packet_buffer);
    else if (opcode != ACK_OPCODE) sendErrorPacket(4, "Illegal TFTP operation, unexpected opcode");
    if (block != expected_block) sendErrorPacket(4, "Illegal TFTP operation, unexpected block");

    printAckPacket(inet_ntoa(server_addr.sin_addr), ntohs(server_addr.sin_port), expected_block, -1, -1);

    return bytes_rx;
}

int handleTimeout(int timeout) {
    fd_set fds;
    struct timeval tv;

    FD_ZERO(&fds);
    FD_SET(sockfd, &fds);

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    int n = select(sockfd + 1, &fds, NULL, NULL, &tv);

    if (n < 0) {
        printError("select failed", true);
    } else if (n == 0) {
        printError("timed out", false);
        return 1; 
    }
    return 0;
}

int main(int argc, char **argv) {
    // Neccessary variables
    char mode[] = "netascii";
    int blksize = DEFAULT_BLKSIZE;
    int timeout = DEFAULT_TIMEOUT;
    int maxRetransmintCount = 3;

    // Variables for command line arguments
    char *host = NULL;
    int server_port = TFTP_SERVER_PORT;
    char *filepath = NULL;
    char *dest_file = NULL;

    handleArguments(argc, argv, &host, &server_port, &filepath, &dest_file);

    createUDPSocket(&sockfd);

    configureServerAddress(host, server_port);

    // Get information about source ip and source port
    socklen_t src_len = sizeof(src_addr);
    if (getsockname(sockfd, (struct sockaddr *)&src_addr, &src_len) < 0) {
        printError("getsockname failed", true);
    }

    // Number of currently processed block
    uint16_t block;

    if (filepath) {
        block = 0;
        int bytes_rx = -1; // bytes received

        sendRqPacket(RRQ_OPCODE, filepath, mode, &blksize, &timeout);

        openFile(mode, filepath, dest_file);

        do {
            for (int i = 0; i <= maxRetransmintCount; i++) {
                if (handleTimeout(timeout)) {
                    if (i == maxRetransmintCount) printError("max retansmission count reached", true);
                    // If the block is zero last attempt to send was to send rq packet, so client has to regransmit rq packet
                    if (block == 0) sendRqPacket(RRQ_OPCODE, filepath, mode, &blksize, &timeout);
                    else sendAckPacket(block);
                } else {
                    break;
                }
            }

            block++;
            bytes_rx = receiveDataPacket(block, blksize);

            // If the client received first data packet update destination port
            if (block == 1) {
                server_port = ntohs(recv_addr.sin_port);
                configureServerAddress(host, server_port);   
            }

            sendAckPacket(block);
            // While not received less data then max in data packet
        } while(bytes_rx >= blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE);

        fclose(file);

    } else {
        block = 0;
        int bytes_tx = -1; // bytes sent

        // Load data from stdin to memory
        int index = 0;
        int ch;
        size_t stdin_data_size = 1024;
        char *stdin_data = (char *)malloc(stdin_data_size * sizeof(char));
        if (stdin_data == NULL) printError("memory allocation error", true);
        while ((ch = getchar()) != EOF) {
            if (index == stdin_data_size) {
                stdin_data_size *= 2;
                stdin_data = (char *)realloc(stdin_data, stdin_data_size * sizeof(char));
                if (stdin_data == NULL) printError("memory reallocation error", true);
            }
            stdin_data[index++] = (char)ch;
        }
        stdin_data[index] = '\0';

        sendRqPacket(WRQ_OPCODE, dest_file, mode, &blksize, &timeout);

        for (int i = 0; i <= maxRetransmintCount; i++)
        {
            if (i == maxRetransmintCount) printError("max retansmission count reached", true);
            if (handleTimeout(timeout)) {
                sendRqPacket(WRQ_OPCODE, dest_file, mode, &blksize, &timeout);
            } else break;
        }

        receiveAckPacket(block);

        block++;

        // Update destination port with src port of ACK with block number 0
        server_port = ntohs(recv_addr.sin_port);
        configureServerAddress(host, server_port);

        do {
            bytes_tx = sendDataPacket(block, blksize, false, stdin_data, (block-1) * blksize);

            for (int i = 0; i <= maxRetransmintCount; i++)
            {
                if (i == maxRetransmintCount) printError("max retansmission count reached", true);
                if (handleTimeout(timeout)) {
                    bytes_tx = sendDataPacket(block, blksize, true, stdin_data, (block-1) * blksize);
                } else break;
            }

            receiveAckPacket(block);

            block++;
            // While not sent less data then max in data packet
        } while(bytes_tx >= blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE);

        // Free allocated memory for stdin data
        if (stdin_data) free(stdin_data);
    }

    closeUDPSocket();

    return 0;
}