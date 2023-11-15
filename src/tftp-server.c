/* tftp-server.c ********************************************************
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
#include "tftp-server.h"

int blksize = DATA_PACKET_SIZE; // default blksize is default TFTP block size
int timeout = 5; // default timeout is 5s

int server_socket = -1;
int sockfd = -1;
struct sockaddr_in server_addr, recv_addr;
socklen_t recv_len = sizeof(recv_addr);

FILE *file;

void printError(char *error) {
    fprintf(stdout, "ERROR: %s\n", error);
    fflush(stdout);
    closeUDPSocket(&server_socket);
    closeUDPSocket(&sockfd);
    fclose(file);
    exit(EXIT_FAILURE);
}

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s [-p port] -f root_dirpath\n", argv[0]);
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

void handleArguments(int argc, char **argv, int *server_port, char **root_dirpath) {
    if (argc < 3 || argc > 5) {
        printUsage(argv);
    }

    char option;
    while ((option = getopt(argc, argv, "p:f:")) != -1) {
        switch (option) {
        case 'p':
            *server_port = atoi(optarg);
            break;
        case 'f':
            *root_dirpath = optarg;
            break;  
        default:
            printUsage(argv);
            break;
        }
    }
}

void createUDPSocket(int *sockfd) {
    *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*sockfd < 0) printError("Creating socket");
}

void closeUDPSocket(int *sockfd) {
    if (*sockfd == -1) return;
    close(*sockfd);
    *sockfd = -1;
}

void configureServerAddress(int server_port) {
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);
}

void openFile(char *root_dirpath, char *mode, char *filename, bool send_file) {
    char filepath[1024] = "";
    strcat(filepath, root_dirpath);
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

void handleOptions(char *rq_packet, size_t bytes_rx) {
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

    while (bytes_processed < bytes_rx) {
        option = &rq_packet[bytes_processed];
        bytes_processed += strlen(option) + 1;

        if (strcmp(option, blcksize_opt)) {
            blksize = atoi(&rq_packet[bytes_processed]);
            blksize = ntohs(blksize);
            blksize += OPCODE_SIZE + BLOCK_NUMBER_SIZE;
            if (blksize < min_blcksize || blksize > max_blcksize) {
                printError("invalid value for blksize option");
            }
        } else if (strcmp(option, timeout_opt)) {
            timeout = atoi(&rq_packet[bytes_processed]);
            timeout = ntohs(timeout);
            if (timeout < min_timeout || timeout > max_timeout) {
                printError("invalid value for timeout option");
            }
        } else {
            printError("invalid option in rq packet");
        }
        bytes_processed += strlen(&rq_packet[bytes_processed]) + 1;
    }
    
}

void handleErrorPacket(char *packet) {
    uint16_t errorCode;
    memcpy(&errorCode, &packet[2], 2);

    char *errMsg = &packet[4];
    printError(errMsg);

    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    if (getsockname(sockfd, (struct sockaddr *)&src_addr, &src_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    printErrorPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), ntohs(src_addr.sin_port), ntohs(errorCode), errMsg);
}

void sendErrorPacket(int errorCode, char *errMsg) {
    uint16_t opcode = ERROR_OPCODE;

    errorCode = htons(errorCode);
    opcode = htons(opcode);

    int error_packet_len = 2 + 2 + strlen(errMsg) + 1;
    char error_buffer[error_packet_len];
    bzero(error_buffer, sizeof(error_buffer));
    memcpy(&error_buffer[0], &opcode, 2);
    memcpy(&error_buffer[2], &errorCode, 2);
    memcpy(&error_buffer[2], errMsg, strlen(errMsg));

    int bytes_tx = sendto(sockfd, error_buffer, error_packet_len, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("sendto not successful");

    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    if (getsockname(sockfd, (struct sockaddr *)&src_addr, &src_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    printErrorPacket(inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), ntohs(recv_addr.sin_port), ntohs(errorCode), errMsg);
}

int receiveRqPacket(char *mode, char *filename, bool *send_file) {
    uint16_t opcode;

    char rq_packet[DATA_PACKET_SIZE];
    bzero(rq_packet, DATA_PACKET_SIZE);

    int bytes_rx = recvfrom(server_socket, rq_packet, DATA_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) perror("recvfrom not succesful");

    memcpy(&opcode, &rq_packet[0], 2);

    opcode = ntohs(opcode);

    if (opcode == ERROR_OPCODE) handleErrorPacket(rq_packet);
    else if (opcode == RRQ_OPCODE) *send_file = true;
    else if (opcode == WRQ_OPCODE) *send_file = false;
    else printError("unexpected opcode while receive eq packet");

    strcpy(filename, &rq_packet[2]);
    strcpy(mode, &rq_packet[2 + strlen(filename) + 1]);

    if (OPCODE_SIZE + strlen(filename) + 1 + strlen(mode) + 1 < bytes_rx) {
        handleOptions(rq_packet, bytes_rx);
    }

    if (opcode == RRQ_OPCODE) {
        printRqPacket("RRQ", inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), filename, mode, blksize, timeout);
    } else if (opcode == WRQ_OPCODE) {
        printRqPacket("WRQ", inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), filename, mode, blksize, timeout);
    }

    return bytes_rx;
}

int sendDataPacket(uint16_t block) {
    uint16_t opcode = DATA_OPCODE;
    
    char data_buffer[blksize];
    bzero(data_buffer, blksize);

    block = htons(block);
    opcode = htons(opcode);

    memcpy(&data_buffer[0], &opcode, 2);
    memcpy(&data_buffer[2], &block, 2);

    int bytes_read = fread(&data_buffer[4], sizeof(char), blksize - OPCODE_SIZE - BLOCK_NUMBER_SIZE, file);

    int bytes_tx = sendto(sockfd, data_buffer, 4 + bytes_read, 0, (struct sockaddr *) &recv_addr, sizeof(recv_addr));
    if (bytes_tx < 0) printError("sendto not successful");

    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    if (getsockname(sockfd, (struct sockaddr *)&src_addr, &src_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    printDataPacket(inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), ntohs(recv_addr.sin_port), ntohs(block));

    return bytes_tx;
}

int receiveDataPacket(uint16_t expected_block) {
    uint16_t expected_opcode = DATA_OPCODE;
    uint16_t block;
    uint16_t opcode;

    char data_buffer[blksize];
    bzero(data_buffer, blksize);

    int bytes_rx = recvfrom(sockfd, data_buffer, blksize, 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    memcpy(&opcode, &data_buffer[0], 2);
    opcode = ntohs(opcode);
    if (opcode == ERROR_OPCODE) handleErrorPacket(data_buffer);
    else if (opcode != expected_opcode) printError("unexpected opcode while receive data");
    memcpy(&block, &data_buffer[2], 2);
    block = ntohs(block);
    if (block != expected_block) printError("unexpected block while receive data");

    char data[blksize - OPCODE_SIZE - BLOCK_NUMBER_SIZE];
    bzero(data, sizeof(data));
    memcpy(data, &data_buffer[4], bytes_rx - 4);

    if (fprintf(file, "%s", data) < 0) printError("appending to file");

    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    if (getsockname(sockfd, (struct sockaddr *)&src_addr, &src_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    printDataPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), ntohs(src_addr.sin_port), ntohs(block));


    return bytes_rx;
}

int sendAckPacket(uint16_t block) {
    uint16_t opcode = ACK_OPCODE;
    char ack_buffer[ACK_PACKET_SIZE];

    block = htons(block);
    opcode = htons(opcode);

    bzero(ack_buffer, 4);
    memcpy(&ack_buffer[0], &opcode, 2);
    memcpy(&ack_buffer[2], &block, 2);

    int bytes_tx = sendto(sockfd, ack_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, sizeof(recv_addr));
    if (bytes_tx < 0) printError("sendto not succesful");

    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    if (getsockname(sockfd, (struct sockaddr *)&src_addr, &src_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    printAckPacket(inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), ntohs(block), -1, -1);

    return bytes_tx;
}

int receiveAckPacket(uint16_t expected_block) {
    uint16_t expected_opcode = ACK_OPCODE;
    uint16_t block;
    uint16_t opcode;
    char ack_buffer[ACK_PACKET_SIZE];

    int bytes_rx = recvfrom(sockfd, ack_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    memcpy(&block, &ack_buffer[2], 2);
    memcpy(&opcode, &ack_buffer[0], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    if (opcode == ERROR_OPCODE) handleErrorPacket(ack_buffer);
    else if (opcode != ACK_OPCODE) printError("unexpected opcode while receive ack");
    if (block != expected_block) printError("unexpected block while receive ack");

    struct sockaddr_in src_addr;
    socklen_t src_len = sizeof(src_addr);
    if (getsockname(sockfd, (struct sockaddr *)&src_addr, &src_len) < 0) {
        perror("getsockname failed");
        exit(EXIT_FAILURE);
    }
    printAckPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), block, -1, -1);

    return bytes_rx;
}

void handleTimeout() {
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
    int server_port = TFTP_SERVER_PORT;
    char *root_dirpath = NULL;

    char mode[20];

    bool send_file;
    char filename[512 - OPCODE_SIZE];

    handleArguments(argc, argv, &server_port, &root_dirpath);

    createUDPSocket(&server_socket);

    configureServerAddress(server_port);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printError("Bind error");
    }

    while(true) {
        receiveRqPacket(mode, filename, &send_file);

        pid_t pid = fork();
        if (pid != 0) {
            closeUDPSocket(&sockfd);
            continue;
        } else {
            closeUDPSocket(&server_socket);

            createUDPSocket(&sockfd);

            uint16_t block;

            if (send_file) {
                openFile(root_dirpath, mode, filename, send_file);

                int bytes_tx;
                block = 1;

                do {
                    while(true);
                    bytes_tx = sendDataPacket(block);

                    handleTimeout();
                    receiveAckPacket(block);

                    block++;
                } while (bytes_tx >= DATA_PACKET_SIZE);
                
            } else
            {
                openFile(root_dirpath, mode, filename, send_file);

                block = 0;
                int bytes_rx;

                sendAckPacket(block);

                block++;

                do {
                    handleTimeout();
                    bytes_rx = receiveDataPacket(block);

                    sendAckPacket(block);

                    block++;
                } while (bytes_rx >= DATA_PACKET_SIZE);
            }

            fclose(file);

            closeUDPSocket(&sockfd);
            break;
        }
    }

    return 0;
}