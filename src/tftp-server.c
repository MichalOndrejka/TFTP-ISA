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

int server_port = TFTP_SERVER_PORT;
char *root_dirpath = NULL;

int server_socket = -1;
struct sockaddr_in client_addr_in;
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
    inet_ntop(AF_INET, &(client_addr_in.sin_addr), source_ip, INET_ADDRSTRLEN);
    int source_port = ntohs(client_addr_in.sin_port);
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
    //Check number of argument
    if (argc < 3 || argc > 5) {
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
    bzero(&client_addr_in, sizeof(client_addr_in));
    client_addr_in.sin_family = AF_INET;
    client_addr_in.sin_addr.s_addr = INADDR_ANY;
    client_addr_in.sin_port = htons(server_port);

    client_addr = (struct sockaddr *) &client_addr_in;
    client_len = sizeof(client_addr_in);

    if (bind(server_socket, client_addr, client_len) < 0) {
        perror("bind error");
        exit(EXIT_FAILURE);
    }
}

void receiveRqPacket() {
    printf("Receiving rq\n");
    int16_t opcode;

    char rq_packet[DATA_PACKET_SIZE];
    bzero(rq_packet, DATA_PACKET_SIZE);

    bytes_rx = recvfrom(server_socket, rq_packet, DATA_PACKET_SIZE, 0, client_addr, &client_len);
    if (bytes_rx < 0) perror("recvfrom not succesful");

    // Check the opcode
    memcpy(&opcode, &rq_packet[0], 2);

    opcode = ntohs(opcode);

    if (opcode == RRQ_OPCODE) send_file = true;
    else if (opcode == WRQ_OPCODE) send_file = false;
    else printError("unexpected opcode while receive eq packet");

    // Get the filename
    strcpy(filename, &rq_packet[2]); // GET FILENAME

    // Get the mode
    strcpy(mode, &rq_packet[2 + strlen(filename) + 1]); // GET MODE
}

void openFile() {
    strcat(filepath, root_dirpath); // Could be problem here
    strcat(filepath, filename);
    printf("path: %s\n", filepath);
    fflush(stdout);
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

void sendDataPacket(int16_t block) {
    printInfo("DATA", block);

    int16_t opcode = DATA_OPCODE;
    
    bzero(data_buffer, DATA_PACKET_SIZE);

    block = htons(block);
    opcode = htons(opcode);

    memcpy(&data_buffer[0], &opcode, 2);
    memcpy(&data_buffer[2], &block, 2);

    bytes_read = fread(&data_buffer[4], 1, DATA_PACKET_SIZE - 4, file);

    bytes_tx = sendto(server_socket, data_buffer, 4 + bytes_read, 0, client_addr, client_len);
    if (bytes_tx < 0) printError("sendto not successful");
}

void receiveDataPacket(int16_t expected_block) {
    printf("Receiving data\n");
    int16_t expected_opcode = DATA_OPCODE;
    int16_t block;
    int16_t opcode;

    bzero(data_buffer, DATA_PACKET_SIZE);
    bytes_rx = recvfrom(server_socket, data_buffer, DATA_PACKET_SIZE, 0, client_addr, &client_len);
    if (bytes_rx < 0) printError("recvfrom not succesful");

    // CHECK OPCODE AND BLOCK NUMBER
    memcpy(&opcode, &data_buffer[0], 2);
    opcode = ntohs(opcode);
    if (opcode != expected_opcode) printError("unexpected opcode while receive data");
    memcpy(&block, &data_buffer[2], 2);
    block = ntohs(block);
    if (block != expected_block) printError("unexpected block while receive data");

    // GET DATA
    memcpy(data, &data_buffer[4], bytes_rx - 4);

    // HANDLE DATA
    if (fprintf(file, "%s", data) < 0) printError("appending to file");
}

void sendAckPacket(int16_t block) {
    printInfo("ACK", block);
    int16_t opcode = ACK_OPCODE;
    char ack_buffer[ACK_PACKET_SIZE];

    block = htons(block);
    opcode = htons(opcode);

    // CREATE ACK PACKET
    bzero(ack_buffer, 4);
    memcpy(&ack_buffer[0], &opcode, 2);
    memcpy(&ack_buffer[2], &block, 2);

    // SEND ACK PACKET
    bytes_tx = sendto(server_socket, ack_buffer, ACK_PACKET_SIZE, 0, client_addr, client_len);
    if (bytes_tx < 0) printError("sendto not succesful");
}

void receiveAckPacket(int16_t expected_block) {
    printf("Receiving ack\n");
    int16_t expected_opcode = ACK_OPCODE;
    int16_t block;
    int16_t opcode;
    char ack_buffer[ACK_PACKET_SIZE];

    bytes_rx = recvfrom(server_socket, ack_buffer, ACK_PACKET_SIZE, 0, client_addr, &client_len);
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

    createUDPSocket();

    configureServerAddress();

    while(true) {
        receiveRqPacket();

        pid_t pid = fork();
        if (pid != 0) {
            while (true)
            {
                /* code */
            }
            
            continue;
        } else {
            int16_t block;

            if (send_file) {
                openFile();

                block = 1;

                do {
                    sendDataPacket(block);

                    receiveAckPacket(block);

                    block++;
                    printf("%d\n", bytes_read);
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

            break;
        }
    }

    return 0;
}