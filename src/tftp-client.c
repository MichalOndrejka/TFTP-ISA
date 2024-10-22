/* tftp-client.c ********************************************************
 * Name: Michal
 * Surname: Ondrejka
 * Login: xondre15
 * **********************************************************************
 */

#include "../include/tftp-client.h"

int sockfd = -1;
struct sockaddr_in server_addr, recv_addr, src_addr;
socklen_t recv_len = sizeof(recv_addr);

FILE *file = NULL;

// Function for printing error messages and terminating process if exit_failure
void printError(char *error, bool exit_failure) {
    fprintf(stdout, "Local error: %s\n", error);
    fflush(stdout);
    if (exit_failure) {
        if (file) fclose(file);
        closeUDPSocket();
        exit(EXIT_FAILURE);
    }
}

// Function for printing usage and terminating process
void printUsage(char **argv) {
    fprintf(stdout, "Usage: %s -h <hostname> [-p port] [-f filepath] -t <dest_filepath>\n", argv[0]);
    exit(EXIT_FAILURE);
}

// Function for printing RQ packets
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

// Function for printing ACK and OACK packet (OACK is when block_id == -1)
void printAckPacket(char *scr_ip, int src_port, int block_id, int blksize, int timeout) {
    // Format OPTS output to be appended after OACK packet
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

// Fucntion for printing DATA packet
void printDataPacket(char *src_ip, int src_port, int dest_port, int block_id) {
    fprintf(stderr, "DATA %s:%d:%d %d", src_ip, src_port, dest_port, block_id);
    fprintf(stderr, "\n");
    fflush(stderr);
}

// Fucntion for printing ERROR packet
void printErrorPacket(char *src_ip, int src_port, int dest_port, int code, char *message) {
    fprintf(stderr, "ERROR %s:%u:%d %d \"%s\"", src_ip, src_port, dest_port, code, message);
    fprintf(stderr, "\n");
    fflush(stderr);
}

// Debug function for printing packets
void printPacket(char *packet, int size) {
    fprintf(stdout, "Packet size: %d\n", size);
    for (int i = 0; i < size; i++) {
        if (packet[i] <= 9) printf("%d ", packet[i]);
        else (printf("%c ", packet[i]));
    }
    printf("\n");
}

// Function for handling arguments
void handleArguments(int argc, char **argv, char **host, int *server_port, char **filepath, char **dest_file) {
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

    if (*host == NULL) printUsage(argv);
    if (*dest_file == NULL) printUsage(argv);
}

// Function for creating udp socket and saving the fd to sockfd
void createUDPSocket(int *sockfd) {
    *sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (*sockfd < 0) printError("couldn't create scoket", true);
}

// Function for closing socket fd
void closeUDPSocket() {
    if (sockfd == -1) return;
    close(sockfd);
    sockfd = -1;
}

// Configure address to IPv4, set address obtained from host name and set port
void configureServerAddress(char *host, int server_port) {
    // Resolve host
    struct hostent *host_info = gethostbyname(host);
    if (host_info == NULL) printError("no such host", true);

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    memcpy(&server_addr.sin_addr.s_addr, host_info->h_addr_list[0], host_info->h_length);
}

/**
 * @brief Open file for write
 *
 * @param dest_file destination path, open file on this path
 */
void openFile(char *dest_file) {
    file = fopen(dest_file, "wb");
    if (file == NULL) printError("creating file", true);
}

/**
 * @brief Send error packet. Set opcode, error code and error message
 *
 * @param error_code error packet
 * @param error_msg error message
 */
int sendErrorPacket(uint16_t error_code, char *error_msg) {
    uint16_t opcode = ERROR_OPCODE;

    opcode = htons(opcode);
    error_code = htons(error_code);

    // Set error packet
    int packet_buffer_len = OPCODE_SIZE + EEROR_CODE_SIZE + strlen(error_msg) + 1;
    char packet_buffer[packet_buffer_len];
    bzero(packet_buffer, sizeof(packet_buffer));
    memcpy(&packet_buffer[0], &opcode, 2);
    memcpy(&packet_buffer[2], &error_code, 2);
    memcpy(&packet_buffer[4], error_msg, strlen(error_msg));

    // Send error packet
    int bytes_tx = sendto(sockfd, packet_buffer, packet_buffer_len, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("sendto not successful", true);

    // Print local error
    if (error_code == 5) printError(error_msg, false);
    else printError(error_msg, true);

    return bytes_tx;
}

/**
 * @brief Handler for error packet. If error code isn't 5 terminate process
 *
 * @param packet error packet
 */
void handleErrorPacket(char *packet) {
    uint16_t error_code;

    // Get code and message
    memcpy(&error_code, &packet[2], 2);
    error_code = ntohs(error_code);
    char *error_msg = &packet[4];

    // Print ERROR packet
    printErrorPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), ntohs(src_addr.sin_port), error_code, error_msg);

    // Print local error
    if (error_code == 5) printError(error_msg, false);
    else printError(error_msg, true);
}

/**
 * @brief Receive OACK packet with blksize and timeout
 *
 * @param blksize get blksize if in options
 * @param timeout get timeout if in options
 * 
 * @return bytes received
 */
int receiveOackPacket(int *blksize, int *timeout) {
    char packet_buffer[DEFAULT_BLKSIZE];
    bzero(packet_buffer, sizeof(packet_buffer));

    // Receive OACK packet
    int bytes_rx = recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) perror("recvfrom not succesful");
    if (bytes_rx < 2) sendErrorPacket(4, "Illegal TFTP operation.");

    // Get opcode
    uint16_t opcode;
    memcpy(&opcode, &packet_buffer[0], 2);
    opcode = ntohs(opcode);

    // Check opcode
    if (opcode == ERROR_OPCODE) handleErrorPacket(packet_buffer);
    else if (opcode != OACK_OPCODE) sendErrorPacket(4, "Illegal TFTP operation.");

    char blksize_opt[] = "blksize";
    int blksize_val = DEFAULT_BLKSIZE;

    char timeout_opt[] = "timeout";
    int timeout_val = DEFAULT_TIMEOUT;

    int bytes_processed = OPCODE_SIZE;

    char *option;
    char *value;

    // Loop through pairs of option and value and save them if recognized
    while (bytes_processed < bytes_rx) {
        option = &packet_buffer[bytes_processed];
        bytes_processed += strlen(option) + 1;
        value = &packet_buffer[bytes_processed];
        bytes_processed += strlen(value) + 1;

        if (!strcmp(option, blksize_opt)) {
            blksize_val = atoi(value);
            *blksize = blksize_val;
            if (*blksize < MIN_BLKSIZE || *blksize > MAX_BLKSIZE) {
                printError("invalid value for blksize option", true);
            }
        } else if (!strcmp(option, timeout_opt)) {
            timeout_val = atoi(value);
            *timeout = timeout_val;
            if (*timeout < MIN_TIMEOUT || *timeout > MAX_TIMEOUT) {
                printError("invalid value for timeout option", true);
            }
        }
    }

    printAckPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), -1, *blksize, *timeout);

    return bytes_rx;
}

/**
 * @brief Send RQ packet, set opcode (RRQ/WRQ), set filename, set mode and append options if any
 *
 * @param opcode RRQ or WRQ opcode
 * @param filename name of file
 * @param mode mode to be set in rq packet
 * @param blksize set blksize if any in options
 * @param timeout set timeout if any in options
 * 
 * @return bytes sent
 */
int sendRqPacket(uint16_t opcode, char *filename, char *mode, int *blksize, int *timeout) {
    opcode = htons(opcode);

    int opts_len = 0;

    // For formating blksize option
    char blksize_opt[] = "blksize";
    char blksize_val[64];
    bzero(blksize_val, sizeof(blksize_val));
    sprintf(blksize_val, "%d", *blksize);

    // For formating timeout option
    char timeout_opt[] = "timeout";
    char timeout_val[64];
    bzero(timeout_val, sizeof(timeout_val));
    sprintf(timeout_val, "%d", *timeout);

    // Calculate length of options
    if (*blksize != DEFAULT_BLKSIZE) opts_len += strlen(blksize_opt) + 1 + strlen(blksize_val) + 1;
    if (*timeout != DEFAULT_TIMEOUT) opts_len += strlen(timeout_opt) + 1 + strlen(timeout_val) + 1;

    // Create packet
    int packet_buffer_len = 2 + strlen(filename) + 1 + strlen(mode) + 1 + opts_len;
    char packet_buffer[packet_buffer_len];
    bzero(packet_buffer, sizeof(packet_buffer));

    // Set packet
    int curr_byte = 0;
    memcpy(&packet_buffer[curr_byte], &opcode, 2);
    curr_byte += 2;
    memcpy(&packet_buffer[curr_byte], filename, strlen(filename));
    curr_byte += strlen(filename) + 1;
    memcpy(&packet_buffer[curr_byte], mode, strlen(mode));
    curr_byte += strlen(mode) + 1;

    // Set options if any
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

    // Send packet
    int bytes_tx = sendto(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("rq packet sendto failed", true);

    return bytes_tx;
}

/**
 * @brief Send DATA packet, set opcode, read blksize bytes of data and set it in packet
 *
 * @param block expected block number of data
 * @param blksize size of payload data
 * @param stdin_data string containing data from stdin
 * @param stdin_data_index from what index in stdin_data should be sent blksize bytes of data
 * 
 * @return bytes sent
 */
int sendDataPacket(uint16_t block, uint16_t blksize, char *stdin_data, int stdin_data_index) {
    uint16_t opcode = DATA_OPCODE;

    // Create DATA packet
    char packet_buffer[OPCODE_SIZE + BLOCK_NUMBER_SIZE + blksize];
    bzero(packet_buffer, sizeof(packet_buffer));

    opcode = htons(opcode);
    block = htons(block);

    // Set opcode and block number
    memcpy(&packet_buffer[0], &opcode, 2);
    memcpy(&packet_buffer[2], &block, 2);

    // Set data
    int bytes_read = strlen(&stdin_data[stdin_data_index]);
    if (bytes_read > blksize) bytes_read = blksize;
    memcpy(&packet_buffer[4], &stdin_data[stdin_data_index], bytes_read);

    // Send packet
    int bytes_tx = sendto(sockfd, packet_buffer, bytes_read + OPCODE_SIZE + BLOCK_NUMBER_SIZE, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
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
int receiveDataPacket(uint16_t expected_block, uint16_t blksize) {
    // Create packet
    char packet_buffer[blksize + OPCODE_SIZE + BLOCK_NUMBER_SIZE];
    bzero(packet_buffer, sizeof(packet_buffer));
    
    // Receive packet
    int bytes_rx = recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("data packet recvfrom failed", true);    
    if (bytes_rx < 4) printError("too little bytes in data packet recvfrom", true);

    uint16_t opcode;
    uint16_t block;

    // Get opcode and block number
    memcpy(&opcode, &packet_buffer[0], 2);
    memcpy(&block, &packet_buffer[2], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    // Check opcode and block
    if (opcode == ERROR_OPCODE) handleErrorPacket(packet_buffer);
    else if (opcode != DATA_OPCODE) sendErrorPacket(4, "Illegal TFTP operation.");

    if (block != expected_block) sendErrorPacket(4, "Illegal TFTP operation.");

    // Write data to a file
    char data[blksize + 1];
    bzero(data, sizeof(data));
    memcpy(data, &packet_buffer[4], bytes_rx - 4);
    if (fprintf(file, "%s", data) < 0) sendErrorPacket(3, "Disk full or allocation exceeded");

    // Print DATA packet
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

    // Create ACK packet
    char packet_buffer[ACK_PACKET_SIZE];
    bzero(packet_buffer, 4);

    opcode = htons(opcode);
    block = htons(block);

    // Set ACK packet
    memcpy(&packet_buffer[0], &opcode, 2);
    memcpy(&packet_buffer[2], &block, 2);

    // Send packet
    int bytes_tx = sendto(sockfd, packet_buffer, ACK_PACKET_SIZE, 0, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (bytes_tx < 0) printError("ack packet sendto failed", true);

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
    char packet_buffer[DEFAULT_BLKSIZE];
    bzero(packet_buffer, sizeof(packet_buffer));

    // Receive ACK buffer
    int bytes_rx = recvfrom(sockfd, packet_buffer, sizeof(packet_buffer), 0, (struct sockaddr *) &recv_addr, &recv_len);
    if (bytes_rx < 0) printError("recvfrom not succesful", true);
    if (bytes_rx < 4) sendErrorPacket(4, "Illegal TFTP operation.");

    uint16_t opcode;
    uint16_t block;

    // Get block and opcode
    memcpy(&opcode, &packet_buffer[0], 2);
    memcpy(&block, &packet_buffer[2], 2);

    opcode = ntohs(opcode);
    block = ntohs(block);

    // Check opcode and block
    if (opcode == ERROR_OPCODE) handleErrorPacket(packet_buffer);
    else if (opcode != ACK_OPCODE) sendErrorPacket(4, "Illegal TFTP operation, unexpected opcode");
    if (block != expected_block) sendErrorPacket(4, "Illegal TFTP operation, unexpected block");

    // Print packet
    printAckPacket(inet_ntoa(recv_addr.sin_addr), ntohs(recv_addr.sin_port), expected_block, -1, -1);

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
    char mode[] = "octet";
    int blksize = DEFAULT_BLKSIZE;
    int timeout = DEFAULT_TIMEOUT;
    int maxRetransmitCount = 3;

    // Variables for command line arguments
    char *host = NULL;
    int server_port = TFTP_SERVER_PORT;
    char *filepath = NULL;
    char *dest_file = NULL;

    handleArguments(argc, argv, &host, &server_port, &filepath, &dest_file);

    createUDPSocket(&sockfd);

    configureServerAddress(host, server_port);

    // Get information about source ip and source port
    if (bind(sockfd, (struct sockaddr*)&src_addr, sizeof(src_addr)) < 0) {
        printError("bind failed", true);
    }

    // Get information about the source IP and source port after the socket is bound
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

        openFile(dest_file);

        if (blksize != DEFAULT_BLKSIZE || timeout != DEFAULT_TIMEOUT) {
            for (int i = 0; i <= maxRetransmitCount; i++)
            {
                if (i == maxRetransmitCount) printError("max retansmission count reached", true);
                if (handleTimeout(timeout)) {
                    sendRqPacket(RRQ_OPCODE, filepath, mode, &blksize, &timeout);
                } else break;
            }

            receiveOackPacket(&blksize, &timeout);

            server_port = ntohs(recv_addr.sin_port);
            configureServerAddress(host, server_port);

            sendAckPacket(block);
        }

        do {
            for (int i = 0; i <= maxRetransmitCount; i++) {
                if (handleTimeout(timeout)) {
                    if (i == maxRetransmitCount) printError("max retansmission count reached", true);
                    // If the block is zero last attempt to send was to send rq packet, so client has to regransmit rq packet
                    if (block == 0) {
                        if (blksize != DEFAULT_BLKSIZE || timeout != DEFAULT_TIMEOUT) sendAckPacket(block);
                        else sendRqPacket(RRQ_OPCODE, filepath, mode, &blksize, &timeout);
                    }
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

        for (int i = 0; i <= maxRetransmitCount; i++)
        {
            if (i == maxRetransmitCount) printError("max retansmission count reached", true);
            if (handleTimeout(timeout)) {
                sendRqPacket(WRQ_OPCODE, dest_file, mode, &blksize, &timeout);
            } else break;
        }

        if (blksize != DEFAULT_BLKSIZE || timeout != DEFAULT_TIMEOUT) receiveOackPacket(&blksize, &timeout);
        else receiveAckPacket(block);

        block++;

        // Update destination port with src port of ACK with block number 0
        server_port = ntohs(recv_addr.sin_port);
        configureServerAddress(host, server_port);

        do {
            bytes_tx = sendDataPacket(block, blksize, stdin_data, (block-1) * blksize); // last argument calculates what index should data start on

            for (int i = 0; i <= maxRetransmitCount; i++)
            {
                if (i == maxRetransmitCount) printError("max retansmission count reached", true);
                if (handleTimeout(timeout)) {
                    bytes_tx = sendDataPacket(block, blksize, stdin_data, (block-1) * blksize);
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