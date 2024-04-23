/*
Authors: Bhuvaneshvar Narayan J H, Jahnavi Mandadi
Subject: Advanced Systems Programming
University of Windsor
Project
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>
#include <ctype.h> // Include ctype.h for character type functions

#define PORT 12345  // The port number to connect to the server on
#define BUFFER_SIZE 1024
#define MAX_RETRIES 5
int sockfd = -1;
struct sockaddr_in serv_addr; // Struct for server address details
struct hostent *server; // Struct to hold info about the host/server

// Function to handle errors throughout the program
void error(const char *msg, int errnum) {
    // Print the error message and corresponding system error string to stderr
    fprintf(stderr, "%s: %s\n", msg, strerror(errnum));
    // Check if the socket descriptor is valid to avoid closing an invalid file descriptor
    if(sockfd >= 0) {
        close(sockfd);  // Close the socket descriptor to free up resources
    }
    // Handle specific network-related errors with potential for recovery
    if(errnum == ECONNRESET || errnum == ETIMEDOUT || errnum == ECONNREFUSED) {
        fprintf(stderr, "Attempting to reconnect...\n");  // Inform the user about reconnection attempt
        // Try to reconnect to the server
        if (!connect_to_server()) {
            // If reconnection fails, print error message and exit
            fprintf(stderr, "Failed to reconnect. Exiting...\n");
            exit(1);  // Exit the program with a status of 1 indicating an error
        }
    } else {
        // For all other types of errors, just exit the program
        exit(1);  // Exit the program with a status of 1 indicating an error
    }
}
// Function to connect to a server with retry logic
int connect_to_server() {
     int retries = 0; // Initialize retry counter
    sockfd = socket(AF_INET, SOCK_STREAM, 0); // Create socket
    if (sockfd < 0) { // Check if socket creation failed
        fprintf(stderr, "ERROR opening socket\n"); // Print error message
        return 0; // Return 0 indicating failure
    }

    server = gethostbyname("localhost"); // Replace with actual hostname
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        return 0;
    }
    // Clear serv_addr structure and set address family and port
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy((char *)&serv_addr.sin_addr.s_addr, (char *)server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(PORT);

    while (retries < MAX_RETRIES) {
        if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
            fprintf(stderr, "ERROR connecting: %s\n", strerror(errno));
            retries++;
            sleep(1); // Wait a second before trying again
            continue;
        }
        return 1; // Successfully connected
    }

    return 0; // Failed to connect after retries
}

// Converts date string to time_t, improved error handling
time_t convertDateToTimestamp(const char* dateStr) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    if (strptime(dateStr, "%Y-%m-%d", &tm) == NULL) {
        fprintf(stderr, "Invalid date format.\n");
        return -1; // Return -1 on failure
    }
    return mktime(&tm);
}
// Case-insensitive string comparison for sorting
int alphaCompare(const void *a, const void *b) {
    const char *strA = *(const char **)a;
    const char *strB = *(const char **)b;
    return strcasecmp(strA, strB);
}

// Time comparison for sorting files based on modification time
int timeCompare(const void *a, const void *b) {
    struct stat statA, statB;
    if (stat(*(const char **)a, &statA) != 0 || stat(*(const char **)b, &statB) != 0) {
        perror("Stat failed");
        return 0; // Consider how to handle comparison failure
    }
    return (statA.st_mtime > statB.st_mtime) - (statA.st_mtime < statB.st_mtime);
}
// Helper function to verify file extensions
int verifyExtensions(char *exts) {
    int count = 0;
    char *token = strtok(exts, " ");
    while (token != NULL) {
        if (!isalpha(token[0])) return 0; // Simple check for valid extension
        count++;
        if (count > 3) return 0; // No more than three extensions
        token = strtok(NULL, " ");
    }
    return count > 0; // At least one extension must be present
}
// Function to verify general date format and values
int verifyDate(const char* date) {
    int y, m, d;
    if (sscanf(date, "%d-%d-%d", &y, &m, &d) != 3) {
        printf("Invalid date format. Use 'YYYY-MM-DD'.\n");
        return 0;
    }
    if (y < 1900 || y > 2100) {  // Assuming your application should only handle dates within this range
        printf("Year must be between 1900 and 2100.\n");
        return 0;
    }
    if (m < 1 || m > 12) {
        printf("Month must be between 1 and 12.\n");
        return 0;
    }
    if (d < 1 || d > 31) {
        printf("Day must be between 1 and 31 based on the month and year.\n");
        return 0;
    }
    // Additional logic to handle different months and leap years could be added here
    return 1;
}

// Wrapper functions that simply call verifyDate
int verifyW24fdb(const char* date) { // Function to verify a date for a W24fdb command
    return verifyDate(date);
}

int verifyW24fda(const char* date) { // Function to verify a date for a W24fda command
    return verifyDate(date);
}

int writeFully(int sockfd, const char *buffer, size_t length) { // Function to write data to socket until all bytes are written
    size_t totalWritten = 0;
    ssize_t bytesWritten;
    while (totalWritten < length) {
        bytesWritten = write(sockfd, buffer + totalWritten, length - totalWritten);
        if (bytesWritten < 0) error("ERROR writing to socket", errno);
        totalWritten += bytesWritten;
    }
}
void sendCommand(int sockfd, const char* command) { // Function to send a command over the socket
    if (write(sockfd, command, strlen(command)) < 0) {
        error("ERROR writing to socket", errno);
    }
}
void ensure_w24project_directory_exists() {
    char path[1024];
    snprintf(path, sizeof(path), "%s/w24project", getenv("HOME"));  // Path to w24project in the home directory
    struct stat st = {0};

    if (stat(path, &st) == -1) {
        mkdir(path, 0700);  // Create the directory if it does not exist
    }
}

void handleServerResponse(int sockfd) {
    char response[BUFFER_SIZE];
    int bytes_read;

    // Set a timeout for the read function
    struct timeval tv;
    tv.tv_sec = 10;  // 10 second timeout
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    bytes_read = read(sockfd, response, BUFFER_SIZE - 1);  // Leave space for null terminator
    if (bytes_read < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            printf("Timeout reached, no more data received from server.\n");
        } else {
            perror("ERROR reading from socket");
        }
        return;
    }

    response[bytes_read] = '\0';  // Properly null-terminate the string

    // Check if the response contains only printable characters
    int is_binary = 0;
    for (int i = 0; i < bytes_read; i++) {
        if (!isprint(response[i]) && !isspace(response[i])) {
            is_binary = 1;
            break;
        }
    }

    if (is_binary) {
        // Assume binary data, save to a file
        ensure_w24project_directory_exists();  // Ensure the directory exists
        printf("Received binary data, saving to w24project/received_files.tar.gz\n");
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/w24project/received_files.tar.gz", getenv("HOME"));
        FILE *fp = fopen(file_path, "wb");
        if (fp == NULL) {
            perror("Failed to open file");
            return;
        }
        fwrite(response, 1, bytes_read, fp);
        // Read the rest of the data and write to file
        while ((bytes_read = read(sockfd, response, BUFFER_SIZE)) > 0) {
            fwrite(response, 1, bytes_read, fp);
        }
        fclose(fp);
    } else {
        // Data is text, print it
        printf("%s\n", response);
    }
}
// Function to verify directory listing commands
int verifyDirlist(const char* cmd) {
    if (strcmp(cmd, "dirlist -a") == 0) {
        return 1; // Valid command for alphabetical listing
    } else if (strcmp(cmd, "dirlist -t") == 0) {
        return 1; // Valid command for time-based listing
    }
    printf("Invalid directory listing command. Use 'dirlist -a' or 'dirlist -t'.\n");
    return 0;
}

// Function to verify the w24fn command
int verifyW24fn(const char* filename) {
    if (filename[0] == '\0') {
        printf("Filename cannot be empty. Use 'w24fn <filename>'.\n");
        return 0;
    }
    // Further checks for filename validity can be implemented here
    return 1;
}

// Function to verify the w24fz command
int verifyW24fz(const char* sizes) {
    int size1, size2;
    if (sscanf(sizes, "%d %d", &size1, &size2) == 2) {
        if (size1 <= size2 && size1 >= 0 && size2 >= 0) {
            return 1;
        }
        printf("Size range is invalid. Ensure that 0 <= size1 <= size2.\n");
    } else {
        printf("Invalid format for w24fz. Use 'w24fz <size1> <size2>'.\n");
    }
    return 0;
}

// Function to verify the w24ft command
int verifyW24ft(const char* extensions) {
    int count = 0;
    char ext[10]; // Assuming a single extension won't be longer than 9 characters + null terminator
    const char* p = extensions;

    while (*p && count < 4) { // Allows a maximum of three extensions
        if (sscanf(p, "%9s", ext) == 1) {
            count++;
            p += strlen(ext);
            while (isspace(*p)) p++; // Skip spaces
        } else {
            break;
        }
    }
    if (count > 0 && count <= 3) {
        return 1;
    }
    printf("Invalid or too many file extensions. Use 'w24ft <ext1> [ext2] [ext3]', up to 3 extensions.\n");
    return 0;
}

// Main command verification function
int verifyCommand(const char* cmd) {
    if (strncmp(cmd, "dirlist ", 8) == 0) {
        return verifyDirlist(cmd);
    } else if (strncmp(cmd, "w24fn ", 6) == 0) {
        return verifyW24fn(cmd + 6);
    } else if (strncmp(cmd, "w24fz ", 6) == 0) {
        return verifyW24fz(cmd + 6);
    } else if (strncmp(cmd, "w24ft ", 6) == 0) {
        return verifyW24ft(cmd + 6);
    } else if (strncmp(cmd, "w24fdb ", 7) == 0) {
        return verifyW24fdb(cmd + 7);
    } else if (strncmp(cmd, "w24fda ", 7) == 0) {
        return verifyW24fda(cmd + 7);
    } else if (strcmp(cmd, "quitc") == 0) {
        return 1; // Direct match for quitting
    }
    printf("Unknown or unsupported command.\n");
    return 0;
}
// Main function to establish connection with the server and handle client-side interactions
int main(int argc, char *argv[]) {
    int sockfd; // Socket file descriptor
    struct sockaddr_in serv_addr;
    struct hostent *server;

    if (argc != 3) { // Check command line arguments
        fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
        exit(1);
    }

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket", errno);
        
    // Get server details
    server = gethostbyname(argv[1]);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(1);
    }

    // Setup server address structure
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr_list[0], 
      (char *)&serv_addr.sin_addr.s_addr,
      server->h_length);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
        error("ERROR connecting", errno);

    char buffer[BUFFER_SIZE]; // Buffer to hold user input
    while (1) { // Loop for user interaction
        printf("clientw24$: ");
        bzero(buffer, BUFFER_SIZE);
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0;  // Remove newline character

        if (strcmp(buffer, "quitc") == 0) {
            sendCommand(sockfd, buffer);  // Send quit command
            break;  // Exit loop
        }

        if (verifyCommand(buffer)) {
            sendCommand(sockfd, buffer);  // Send verified command
            handleServerResponse(sockfd);  // Handle response
        } else {
            printf("Invalid command syntax.\n");
        }
    }

    close(sockfd);  // Close the socket connection
    return 0;
}