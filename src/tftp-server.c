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
#define DATA_PACKET_SIZE 512
#define ACK_PACKET_SIZE 4
#define OPCODE_SIZE 2
#define BLOCK_NUMBER_SIZE 2   
#define RRQ_OPCODE 1
#define WRQ_OPCODE 2
#define DATA_OPCODE 3
#define ACK_OPCODE 4
#define ERROR_OPCODE 5

int server_port = TFTP_SERVER_PORT;
char *root_dirpath = NULL;

int server_socket = -1;
int new_socket = -1;
struct sockaddr *client_addr;
socklen_t client_len;

int bytes_rx;
int bytes_tx;
int bytes_read;

char data_buffer[DATA_PACKET_SIZE];
char data[DATA_PACKET_SIZE - OPCODE_SIZE - BLOCK_NUMBER_SIZE];
char mode[20];

bool send_file;


char filename[512 - OPCODE_SIZE];
FILE *file;

void printError(char *error) {
    fprintf(stderr, "ERROR: %s\n", error);
    exit(EXIT_FAILURE);
}

void printUsage(char **argv) {
    fprintf(stderr, "Usage: %s [-p port] -f root_dirpath\n", argv[0]);
    exit(EXIT_FAILURE);
}

void handleArgument(int argc, char **argv) {
    //Check number of argument
    if (argc < 5 || argc > 9) {
        printUsage(argv);
    }

    char option;
    //Get values of arguments
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

void createUDPSocket() {
    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket < 0) printError("Creating socket");
}

void configureServerAddress() {
    struct sockaddr_in client_addr_in;
    memset(&client_addr_in, 0, sizeof(client_addr_in));
    client_addr_in.sin_family = AF_INET;
    client_addr_in.sin_port = htons(server_port);
    client_addr_in.sin_addr.s_addr = INADDR_ANY;

    client_addr = (struct sockaddr *) &client_addr_in;
    client_len = sizeof(client_addr);

    if (bind(server_socket, client_addr, client_len) < 0) {
        printError("bind error");
    }

    if ((listen(server_socket, 10)) < 0) {
        perror("listen");
    }
}

void receiveRqPacket() {
    int16_t block;
    int16_t opcode;

    char rq_packet[DATA_PACKET_SIZE];

    bzero(rq_packet, DATA_PACKET_SIZE);
    bytes_rx = recvfrom(new_socket, rq_packet, DATA_PACKET_SIZE, 0, client_addr, &client_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    // Check the opcode
    memcpy(&opcode, &rq_packet[0], 2);
    if (opcode == RRQ_OPCODE) send_file = true;
    else if (opcode == WRQ_OPCODE) send_file = false;
    else printError("unexpected opcode");

    // Get the filename
    strcpy(filename, &rq_packet[2]); // GET FILENAME

    // Get the mode
    strcpy(mode, &rq_packet[2 + strlen(filename) + 1]); // GET MODE
}

void openFile() {
    if (send_file) {
        if (strcmp(mode, "netascii")) {
            file = fopen(filename, "r");
        } else if (strcmp(mode, "octet")) {
            file = fopen(filename, "rb");
        }
        if (file == NULL) printError("opening file for read");
    } else {
        file = fopen(filename, "w");
        if (file == NULL) printError("creating file");
        fclose(file);

        if (strcmp(mode, "netascii")) {
            file = fopen(filename, "a");
        } else if (strcmp(mode, "netascii")) {
            file = fopen(filename, "ab");
        }
        if (file == NULL) printError("opening file for append");
    }
}

void sendDataPacket(int16_t block) {
    int16_t opcode = DATA_OPCODE;
    
    bzero(data_buffer, DATA_PACKET_SIZE);

    bytes_read = fread(&data_buffer[4], DATA_PACKET_SIZE - 4, DATA_PACKET_SIZE - 4, file);

    memcpy(&data_buffer[0], &opcode, 2);
    memcpy(&data_buffer[2], &block, 2);

    bytes_tx = sendto(new_socket, data_buffer, 4 + bytes_read, 0, client_addr, client_len);
    if (bytes_tx < 0) printError("sendto not successful");
}

void receiveAckPacket(int16_t expected_block) {
    int16_t expected_opcode = ACK_OPCODE;
    int16_t block;
    int16_t opcode;
    char ack_buffer[ACK_PACKET_SIZE];

    bytes_rx = recvfrom(server_socket, ack_buffer, ACK_PACKET_SIZE, 0, client_addr, &client_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    memcpy(&opcode, &ack_buffer[0], 2);
    if (opcode != ACK_OPCODE) printError("unexpected opcode");
    memcpy(&block, &ack_buffer[2], 2);
    if (block != expected_block) printError("unexpected block");
}

int main(int argc, char **argv) {
    handleArgument(argc, argv);

    createUDPSocket();

    configureServerAddress();

    while(true) {
        new_socket = accept(server_socket, (struct sockaddr *) client_addr, &client_len);
        if (new_socket < 0) printError("accept");

        pid_t pid = fork();
        if (pid != 0) { // PARENT PROCESS
            close(new_socket);

            continue;
        } else { // CHILD PROCESS
            close(server_socket);

            receiveRqPacket();

            int16_t block;

            if (send_file) {
                openFile();
                block = 1;
                do {
                    sendDataPacket(block);

                    receiveAckPacket(block);

                    block++;
                } while (bytes_read >= DATA_PACKET_SIZE - 4);
                
            } else
            {
                openFile();
            }
            


            break;
        }
    }
}