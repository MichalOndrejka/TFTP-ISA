#ifndef TFTP_CLEINT_H
#define TFTP_CLEINT_H

#include <stdio.h>
#include <sys/socket.h>

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

// Global variable declarations (if needed outside, otherwise they should be static inside .c file)
extern int sockfd;
extern struct sockaddr_in server_addr, recv_addr;
extern socklen_t recv_len;

extern FILE *file;

// Function declarations
void closeUDPSocket(void);
void printError(char *error);
void printUsage(char **argv);
void printInfo(char *opcode, int16_t block, char *mode, int server_port, char *filename, bool sender_is_client);
void handleArguments(int argc, char **argv, char **host, int *server_port, char **filepath, char **dest_file);
void createUDPSocket(void);
void configureServerAddress(char *host, int server_port);
void printPacket(char *packet, int size);
void sendRqPacket(int16_t opcode, char *mode, char *filename, int *bytes_tx);
void openFile(char *mode, char *filepath, char *dest_file);
void closeFile(void);
void sendDataPacket(int16_t block, int *bytes_tx);
void receiveDataPacket(int16_t expected_block, int *bytes_rx);
void sendAckPacket(int16_t block, int *bytes_tx);
void receiveAckPacket(int16_t expected_block, int *bytes_rx);

#endif // TFTP_H
