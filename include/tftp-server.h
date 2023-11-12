/* tftp-server.h ********************************************************
 * Name: Michal
 * Surname: Ondrejka
 * Login: xondre15
 * **********************************************************************
 */

#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

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

// Global variables
extern int blksize;
extern int timeout;

extern int server_socket;
extern int sockfd;
extern struct sockaddr_in server_addr, client_addr;
extern socklen_t server_len;
extern socklen_t client_len;

extern FILE *file;

// Function declarations
void printError(char *error);
void printUsage(char **argv);
void printPacket(char *packet, int size);
void printInfo(char *opcode, int16_t block, char *mode, char *filename, bool sender_is_server);
void handleArguments(int argc, char **argv, int *server_port, char **root_dirpath);
void closeUDPSocket(int *udp_socket);
void createUDPSocket(int *udp_socket);
void configureServerAddress(int server_port);
void handleOptions(char *rq_packet, size_t bytes_rx);
void receiveRqPacket(size_t *bytes_rx, char *mode, bool *send_file, char *filename);
void openFile(char *root_dirpath, char *mode, bool send_file, char *filename, char *filepath);
void closeFile(void);
void sendDataPacket(int16_t block, size_t *bytes_tx);
void receiveDataPacket(int16_t expected_block, size_t *bytes_rx);
void sendAckPacket(int16_t block, size_t *bytes_tx);
void receiveAckPacket(int16_t expected_block, size_t *bytes_rx);

#endif
