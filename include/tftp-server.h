/* tftp-server.h ********************************************************
 * Name: Michal
 * Surname: Ondrejka
 * Login: xondre15
 * **********************************************************************
 */

#ifndef TFTP_SERVER_H
#define TFTP_SERVER_H

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
#define TFTP_SERVER_PORT 69
#define DEFAULT_TIMEOUT 5

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
void printRqPacket(char *rq_opcode, char *src_ip, int src_port, char *filepath, char *mode, int blksize, int timeout);
void printAckPacket(char *scr_ip, int src_port, int block_id, char *blksize_val, char *timeout_val);
void printDataPacket(char *src_ip, int src_port, int dest_port, int block_id);
void printErrorPacket(char *src_ip, int src_port, int dest_port, int code, char *message);

void handleArguments(int argc, char **argv, int *server_port, char **root_dirpath);
void createUDPSocket(int *sockfd);
void closeUDPSocket(int *sockfd);
void configureServerAddress(int server_port);
void sendErrorPacket(uint16_t error_code, char *error_msg);
void handleErrorPacket(char *packet);
void openFile(char *root_dirpath, char *filename, bool send_file);
int sendOackPacket(int blksize, int timeout);
void handleOptions(char *rq_packet, size_t bytes_rx, int *blksize, int *timeout);
int receiveRqPacket(char *mode, char *filename, bool *send_file, int *blksize, int *timeout);
int sendDataPacket(uint16_t block, int blksize, bool is_retransmit, char *mode);
int receiveDataPacket(uint16_t expected_block, int blksize, char *mode);
int sendAckPacket(uint16_t block);
int receiveAckPacket(uint16_t expected_block);
int handleTimeout(int timeout);

#endif /* TFTP_SERVER_H */