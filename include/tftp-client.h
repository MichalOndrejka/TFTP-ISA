/* tftp-client.h ********************************************************
 * Name: Michal
 * Surname: Ondrejka
 * Login: xondre15
 * **********************************************************************
 */

#ifndef TFTP_CLEINT_H
#define TFTP_CLEINT_H

#include <stdio.h>
#include <sys/socket.h>

#define TFTP_SERVER_PORT 69
#define DATA_PACKET_SIZE 516
#define ACK_PACKET_SIZE 4
#define OPCODE_SIZE 2
#define BLOCK_NUMBER_SIZE 2
#define EEROR_CODE_SIZE 2
#define RRQ_OPCODE 1
#define WRQ_OPCODE 2
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5
#define OACK_OPCODE 7
#define DEFAULT_BLKSIZE 512
#define DEFAULT_TIMEOUT 5

// Global variable declarations (if needed outside, otherwise they should be static inside .c file)
extern int sockfd;
extern struct sockaddr_in server_addr, recv_addr;
extern socklen_t recv_len;

extern FILE *file;

// Function declarations
void closeUDPSocket(void);
void printError(char *error, bool exit_failure);
void printUsage(char **argv);
void printInfo(char *opcode, uint16_t block, char *filename, char *mode, int server_port, bool sender_is_client);
void handleArguments(int argc, char **argv, char **host, int *server_port, char **filepath, char **dest_file);
void createUDPSocket(int *sockfd);
void configureServerAddress(char *host, int server_port);
void printPacket(char *packet, int size);
int sendRqPacket(uint16_t opcode, char *filename, char *mode, int *blksize, int *timeout);
void openFile(char *mode, char *filepath, char *dest_file);
void closeFile(void);
int sendDataPacket(uint16_t block, uint16_t blksize, bool is_retransmit, char *stdin_data, int stdin_data_index);
int receiveDataPacket(uint16_t expected_block, uint16_t blksize);
int sendAckPacket(uint16_t block);
int receiveAckPacket(uint16_t expected_block);

#endif // TFTP_H
