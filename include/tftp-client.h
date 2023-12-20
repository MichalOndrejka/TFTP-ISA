/* tftp-client.h ********************************************************
 * Name: Michal
 * Surname: Ondrejka
 * Login: xondre15
 * **********************************************************************
 */

#ifndef TFTP_CLIENT_H
#define TFTP_CLIENT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DEFAULT_BLKSIZE 512
#define DEFAULT_TIMEOUT 5
#define TFTP_SERVER_PORT 69

#define RRQ_OPCODE 1
#define WRQ_OPCODE 2
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5
#define OACK_OPCODE 6

#define OPCODE_SIZE 2
#define BLOCK_NUMBER_SIZE 2
#define EEROR_CODE_SIZE 2
#define ACK_PACKET_SIZE 4

#define MIN_BLKSIZE 8
#define MAX_BLKSIZE 65464
#define MIN_TIMEOUT 1
#define MAX_TIMEOUT 255

void printError(char *error, bool exit_failure);
void printUsage(char **argv);
void printRqPacket(char *rq_opcode, char *src_ip, int src_port, char *filepath, char *mode, char *blksize_val, char *timeout_val);
void printAckPacket(char *scr_ip, int src_port, int block_id, int blksize, int timeout);
void printDataPacket(char *src_ip, int src_port, int dest_port, int block_id);
void printErrorPacket(char *src_ip, int src_port, int dest_port, int code, char *message);
void printPacket(char *packet, int size);

void handleArguments(int argc, char **argv, char **host, int *server_port, char **filepath, char **dest_file);
void createUDPSocket(int *sockfd);
void closeUDPSocket();
void configureServerAddress(char *host, int server_port);
void openFile(char *mode, char *filepath, char *dest_file);
int receiveOackPacket(int *blksize, int *timeout);
int sendRqPacket(uint16_t opcode, char *filename, char *mode, int *blksize, int *timeout);
int sendErrorPacket(uint16_t error_code, char *error_msg);
void handleErrorPacket(char *packet);
int sendDataPacket(uint16_t block, uint16_t blksize, char *stdin_data, int stdin_data_index);
int receiveDataPacket(uint16_t expected_block, uint16_t blksize);
int sendAckPacket(uint16_t block);
int receiveAckPacket(uint16_t expected_block);
int handleTimeout(int timeout);

#endif /* TFTP_CLIENT_H */
