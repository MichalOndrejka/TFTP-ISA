/* tftp-server.c ********************************************************
 * Name: Michal
 * Surname: Ondrejka
 * Login: xondre15
 * **********************************************************************
 */

#include "tftp-server.h"

int server_socket = -1;
int sockfd = -1;
struct sockaddr_in server_addr, recv_addr, src_addr;
socklen_t recv_len = sizeof(recv_addr);

FILE *file = NULL;

void printError(char *error, bool exit_failure) {
    fprintf(stdout, "Local error: %s\n", error);
    fflush(stdout);
    if (exit_failure) {
        closeUDPSocket(&server_socket);
        closeUDPSocket(&sockfd);
        if (file) fclose(file);
        exit(EXIT_FAILURE);
    }
}

void printUsage(char **argv) {
    fprintf(stdout, "Usage: %s [-p port] root_dirpath\n", argv[0]);
    fflush(stdout);
    exit(EXIT_FAILURE);
}

void printRqPacket(char *rq_opcode, char *src_ip, int src_port, char *filepath, char *mode, int blksize, int timeout) {
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

    fprintf(stderr, "%s %s:%d \"%s\" %s %s", rq_opcode, src_ip, src_port, filepath, mode, opts);
    fprintf(stderr, "\n");
    fflush(stderr);
}

void printAckPacket(char *scr_ip, int src_port, int block_id, char *blksize_val, char *timeout_val) {
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

void handleArguments(int argc, char **argv, int *server_port, char **root_dirpath) {
    char option;
    while ((option = getopt(argc, argv, "p:")) != -1) {
        switch (option) {
        case 'p':
            *server_port = atoi(optarg);
            break;
        default:
            printUsage(argv);
            break;
        }
    }

    if (optind < argc) {
        *root_dirpath = argv[optind];
    } else {
        printUsage(argv);
    }
}

void createUDPSocket(int *sockfd) {
    *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*sockfd < 0) printError("Creating socket", true);
}

void closeUDPSocket(int *sockfd) {
    if (!sockfd) return;
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

/**
 * @brief Send error packet. Set opcode, error code and error message
 *
 * @param error_code error packet
 * @param error_msg error message
 */
void sendErrorPacket(uint16_t error_code, char *error_msg) {
    uint16_t opcode = ERROR_OPCODE;

    opcode = htons(opcode);
    error_code = htons(error_code);

    int packet_buffer_len = OPCODE_SIZE + EEROR_CODE_SIZE + strlen(error_msg) + 1;
    char packet_buffer[packet_buffer_len];
    bzero(packet_buffer, sizeof(packet_buffer));
    memcpy(&packet_buffer[0], &opcode, 2);
    memcpy(&packet_buffer[2], &error_code, 2);

    memcpy(&packet_buffer[4], error_msg, strlen(error_msg));

    int bytes_tx = sendto(sockfd, packet_buffer, packet_buffer_len, 0, (struct sockaddr *) &recv_addr, sizeof(recv_addr));
    if (bytes_tx < 0) printError("sendto not successful", true);

    printErrorPacket(inet_ntoa(src_addr.sin_addr), ntohs(src_addr.sin_port), ntohs(server_addr.sin_port), ntohs(error_code), error_msg);
}

/**
 * @brief Handler for error packet. If error code isn't 5 terminate process
 *
 * @param packet error packet
 */
void handleErrorPacket(char *packet) {
    uint16_t errorCode;
    memcpy(&errorCode, &packet[2], 2);

    char *errMsg = &packet[4];

    printErrorPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), ntohs(src_addr.sin_port), ntohs(errorCode), errMsg);

    if (errorCode == 5) printError(errMsg, false);
    else printError(errMsg, true);
}

/**
 * @brief Open file for read or write based on send_file value. Concat filename after root_dirpath
 *
 * @param root_dirpath root dirpath of files to be read or to be written
 * @param filename name of file
 * @param send_file server is sending file
 */
void openFile(char *root_dirpath, char *filename, bool send_file) {
    char filepath[1024] = "";
    strcat(filepath, root_dirpath);
    if (filepath[strlen(filepath)] != '/') strcat(filepath, "/");
    strcat(filepath, filename);

    if (send_file) {
        file = fopen(filepath, "rb");
        if (file == NULL) sendErrorPacket(1, "File not found");
    } else {
        file = fopen(filepath, "wb");
        if (file == NULL) printError("creating file", true);
    }
}

/**
 * @brief Send OACK packet with blksize and timeout, if they are not default values
 *
 * @param blksize set blksize if not default in options
 * @param timeout set timeout if not default in options
 * 
 * @return bytes sent
 */
int sendOackPacket(int blksize, int timeout) {
    uint16_t opcode = OACK_OPCODE;

    char blksize_opt[] = "blksize";
    char blksize_val[1024];
    bzero(blksize_val, sizeof(blksize_val));
    sprintf(blksize_val, "%d", blksize);

    char timeout_opt[] = "timeout";
    char timeout_val[1024];
    bzero(timeout_val, sizeof(timeout_val));
    sprintf(timeout_val, "%d", timeout);

    opcode = htons(opcode);

    int packet_buffer_len = OPCODE_SIZE;
    if (blksize != DEFAULT_BLKSIZE) packet_buffer_len += strlen(blksize_opt) + 1 + strlen(blksize_val) + 1;
    if (blksize != DEFAULT_TIMEOUT) packet_buffer_len += strlen(timeout_opt) + 1 + strlen(timeout_val) + 1;

    char packet_buffer[packet_buffer_len];
    bzero(packet_buffer, sizeof(packet_buffer));
    int curr_byte = 0;
    memcpy(&packet_buffer[curr_byte], &opcode, OPCODE_SIZE);
    curr_byte += OPCODE_SIZE;

    if (blksize != DEFAULT_BLKSIZE) {
        memcpy(&packet_buffer[curr_byte], blksize_opt, strlen(blksize_opt));
        curr_byte += strlen(blksize_opt) + 1;
        memcpy(&packet_buffer[curr_byte], blksize_val, strlen(blksize_val));
        curr_byte += strlen(blksize_val) + 1;
    }

    if (timeout != DEFAULT_TIMEOUT) {
        memcpy(&packet_buffer[curr_byte], timeout_opt, strlen(timeout_opt));
        curr_byte += strlen(timeout_opt) + 1;
        memcpy(&packet_buffer[curr_byte], timeout_val, strlen(timeout_val));
        curr_byte += strlen(timeout_val) + 1;
    }

    int bytes_tx = sendto(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &recv_addr, sizeof(recv_addr));

    if (bytes_tx < 0) printError("sendto not succesful", true);

    return bytes_tx;
}

/**
 * @brief Handler for options in RQ packet
 *
 * @param rq_packet rq packet
 * @param bytes_rx length of rq packet
 * @param blksize to set blksize if in potions
 * @param timeout to set timeout if in potions
 */
void handleOptions(char *rq_packet, size_t bytes_rx, int *blksize, int *timeout) {
    char blcksize_opt[] = "blksize";
    char timeout_opt[] = "timeout";

    int filename_len = strlen(&rq_packet[2]);
    int mode_len = strlen(&rq_packet[2 + filename_len + 1]);

    int bytes_processed = 2 + filename_len + 1 + mode_len + 1;

    char *option;
    char *value;

    while (bytes_processed < bytes_rx) {
        option = &rq_packet[bytes_processed];
        bytes_processed += strlen(option) + 1;
        value = &rq_packet[bytes_processed];
        bytes_processed += strlen(value) + 1;

        if (!strcmp(option, blcksize_opt)) {
            *blksize = atoi(value);
        } else if (!strcmp(option, timeout_opt)) {
            *timeout = atoi(value);
        }
    }
}

/**
 * @brief Receive RQ packet, check opcode, set parameter and get options if any
 *
 * @param mode to set mode received in rq packet
 * @param filename to set mode filename in rq packet
 * @param send_file to set send_file if server is sending file else false
 * @param blksize get blksize if any in options
 * @param timeout get timeout if any in options
 * 
 * @return bytes received
 */
int receiveRqPacket(char *mode, char *filename, bool *send_file, int *blksize, int *timeout) {
    char packet_buffer[DEFAULT_BLKSIZE];
    bzero(packet_buffer, sizeof(packet_buffer));

    int bytes_rx = recvfrom(server_socket, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) {
        printError("recvfrom not succesful", false);
        return -1;
    }
    if (bytes_rx < 2) {
        printError("too little bytes in receive data packet", false);
        return -1;
    }

    uint16_t opcode;

    memcpy(&opcode, &packet_buffer[0], 2);

    opcode = ntohs(opcode);

    if (opcode == ERROR_OPCODE) return -1;
    else if (opcode == RRQ_OPCODE) *send_file = true;
    else if (opcode == WRQ_OPCODE) *send_file = false;
    else {
        printError("unexpected opcode while receive rq packet", false);
        return -1;
    }

    strcpy(filename, &packet_buffer[2]);
    strcpy(mode, &packet_buffer[2 + strlen(filename) + 1]);

    if (OPCODE_SIZE + strlen(filename) + 1 + strlen(mode) + 1 < bytes_rx) {
        handleOptions(packet_buffer, bytes_rx, blksize, timeout);
    }

    if (opcode == RRQ_OPCODE) {
        printRqPacket("RRQ", inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), filename, mode, *blksize, *timeout);
    } else if (opcode == WRQ_OPCODE) {
        printRqPacket("WRQ", inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), filename, mode, *blksize, *timeout);
    }

    if (*blksize < MIN_BLKSIZE || *blksize > MAX_BLKSIZE) {
        *blksize = DEFAULT_BLKSIZE;
    }
    if (*timeout < MIN_TIMEOUT || *timeout > MAX_TIMEOUT) {
        *timeout = DEFAULT_TIMEOUT;
    }

    return bytes_rx;
}

/**
 * @brief Send DATA packet, set opcode, read blksize bytes of data and set it in packet
 *
 * @param block expected block number of data
 * @param blksize size of payload data
 * @param is_retransmit seek cursor backward in file so new data aren't read
 * 
 * @return bytes sent
 */
int sendDataPacket(uint16_t block, int blksize, bool is_retransmit) {
    uint16_t opcode = DATA_OPCODE;
    
    char data_buffer[blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE];
    bzero(data_buffer, sizeof(data_buffer));

    block = htons(block);
    opcode = htons(opcode);

    memcpy(&data_buffer[0], &opcode, 2);
    memcpy(&data_buffer[2], &block, 2);

    if (is_retransmit) fseek(file, -blksize, SEEK_END);
    int bytes_read = fread(&data_buffer[4], sizeof(char), blksize, file);

    int bytes_tx = sendto(sockfd, data_buffer, bytes_read + OPCODE_SIZE + BLOCK_NUMBER_SIZE, 0, (struct sockaddr *) &recv_addr, sizeof(recv_addr));
    if (bytes_tx < 0) printError("sendto not successful", true);

    return bytes_tx;
}

/**
 * @brief Receive DATA packet, check opcode, receive blksize bytes of payload data and write it to file
 *
 * @param expected_block expected block number of data
 * @param blksize size of payload data
 * 
 * @return bytes received
 */
int receiveDataPacket(uint16_t expected_block, int blksize) {
    char packet_buffer[blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE];
    bzero(packet_buffer, sizeof(packet_buffer));

    int bytes_rx = recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("recvfrom not succesful", true);
    if (bytes_rx < 4) printError("too little bytes in receive data packet", true);

    uint16_t opcode;
    uint16_t block;

    memcpy(&opcode, &packet_buffer[0], 2);
    memcpy(&block, &packet_buffer[2], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    if (opcode == ERROR_OPCODE) handleErrorPacket(packet_buffer);
    else if (opcode != DATA_OPCODE) printError("unexpected opcode while receive data", true);

    if (block != expected_block) printError("unexpected block while receive data", true);

    char data[blksize + 1];
    bzero(data, sizeof(data));
    memcpy(data, &packet_buffer[4], bytes_rx - 4);

    if (fprintf(file, "%s", data) < 0) sendErrorPacket(3, "Disk full or allocation exceeded");

    printDataPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), ntohs(src_addr.sin_port), block);


    return bytes_rx;
}

/**
 * @brief Send ACK packet, set opcode and set block number with block param
 *
 * @param block block number to send the ack for
 * 
 * @return bytes sent
 */
int sendAckPacket(uint16_t block) {
    uint16_t opcode = ACK_OPCODE;
    char ack_buffer[ACK_PACKET_SIZE];

    block = htons(block);
    opcode = htons(opcode);

    bzero(ack_buffer, 4);
    memcpy(&ack_buffer[0], &opcode, 2);
    memcpy(&ack_buffer[2], &block, 2);

    int bytes_tx = sendto(sockfd, ack_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, sizeof(recv_addr));
    if (bytes_tx < 0) printError("sendto not succesful", true);

    return bytes_tx;
}

/**
 * @brief Receive ACK packet, chceck opcode and compare expected block number with received block number
 *
 * @param expected_block expected block number of ack
 * 
 * @return bytes received
 */
int receiveAckPacket(uint16_t expected_block) {
    uint16_t block;
    uint16_t opcode;
    char ack_buffer[ACK_PACKET_SIZE];

    int bytes_rx = recvfrom(sockfd, ack_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("recvfrom not succesful", true);

    memcpy(&block, &ack_buffer[2], 2);
    memcpy(&opcode, &ack_buffer[0], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    if (opcode == ERROR_OPCODE) handleErrorPacket(ack_buffer);
    else if (opcode != ACK_OPCODE) printError("unexpected opcode while receive ack", true);
    if (block != expected_block) printError("unexpected block while receive ack", true);

    printAckPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), block, NULL, NULL);

    return bytes_rx;
}

/**
 * @brief Waits for data to be available to receive
 *
 * @param timeout time to wait (s)
 * 
 * @return 1 if timed out, 0 if data are available to rece
 */
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
    char mode[128] = "";
    int blksize;
    int timeout;
    int maxRetransmitCount = 3;

    // Variables for command line arguments
    int server_port = TFTP_SERVER_PORT;
    char *root_dirpath = NULL;

    bool send_file; // Indicates if server is sending file
    char filename[1024] = ""; // Allocation for storing name of local file

    handleArguments(argc, argv, &server_port, &root_dirpath);

    createUDPSocket(&server_socket);

    configureServerAddress(server_port);

    // Bind server_socket to listen on specific port
    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        printError("Bind error", true);
    }

    while(true) {
        blksize = DEFAULT_BLKSIZE;
        timeout = DEFAULT_TIMEOUT;

        if (receiveRqPacket(mode, filename, &send_file, &blksize, &timeout) == -1) continue;

        // Create a child proccess to handle the request, the main porccess will listen for more requests 
        pid_t pid = fork();
        if (pid != 0) {
            closeUDPSocket(&sockfd);
            continue;
        } else {
            closeUDPSocket(&server_socket);

            createUDPSocket(&sockfd);

            // Get information about source ip and source port
            if (bind(sockfd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
                printError("bind failed", true);
            }

            // Get information about the source IP and source port after the socket is bound
            socklen_t src_len = sizeof(src_addr);
            if (getsockname(sockfd, (struct sockaddr *)&src_addr, &src_len) < 0) {
                printError("getsockname failed", true);
            }
            
            // If handling options send OACK to the client
            if (blksize != DEFAULT_BLKSIZE || timeout != DEFAULT_TIMEOUT) sendOackPacket(blksize, timeout);

            // Number of currently processed block
            uint16_t block;

            if (send_file) {
                // Open file for read
                openFile(root_dirpath, filename, send_file);

                int bytes_tx; // bytes sent
                block = 0;

                // If handling options handle retransmisson of OACK
                if (blksize != DEFAULT_BLKSIZE || timeout != DEFAULT_TIMEOUT) {
                    for (int i = 0; i <= maxRetransmitCount; i++)
                    {
                        if (i == maxRetransmitCount) printError("max retansmission count reached", true);
                        if (handleTimeout(timeout)) {
                            sendOackPacket(blksize, timeout);
                        } else break;
                    }

                    // After sending OACK server needs to receive ACK with block number 0

                    receiveAckPacket(block);
                }

                block++;

                do {
                    bytes_tx = sendDataPacket(block, blksize, false);

                    // Handle retransmisson of DATA packet
                    for (int i = 0; i <= maxRetransmitCount; i++)
                    {
                        if (i == maxRetransmitCount) printError("max retansmission count reached", true);
                        if (handleTimeout(timeout)) {
                            bytes_tx = sendDataPacket(block, blksize, true);
                        } else break;
                    }
                    // Receive appropriate ACK
                    receiveAckPacket(block);

                    block++;
                    // While not sent less data then max in data packet
                } while (bytes_tx >= blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE);
                
            } else {
                // Open file for write
                openFile(root_dirpath, filename, send_file);

                block = 0;
                int bytes_rx; // bytes received

                // If not handling options send ACK to the client
                if ((blksize == DEFAULT_BLKSIZE && timeout == DEFAULT_TIMEOUT)) sendAckPacket(block);

                do {
                    for (int i = 0; i <= maxRetransmitCount; i++)
                    {
                        if (i == maxRetransmitCount) printError("Max retansmission count reached", true);
                        if (handleTimeout(timeout)) {
                            // If this if is true, it means last sent packet was OACK, thus retransmit OACK
                            if (block = 1 && (blksize != DEFAULT_BLKSIZE || timeout != DEFAULT_TIMEOUT)) {
                                sendOackPacket(blksize, timeout);
                            }
                            else sendAckPacket(block);
                        } else break;
                    }

                    block++;

                    bytes_rx = receiveDataPacket(block, blksize);

                    sendAckPacket(block);
                // While not received less data then max in data packet
                } while (bytes_rx >= blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE);
            }
            break;
        }
    }

    return 0;
}