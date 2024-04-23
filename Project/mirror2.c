/*
Authors: Bhuvaneshvar Narayan J H, Jahnavi Mandadi
Subject: Advanced Systems Programming
University of Windsor
Project
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#ifndef DT_DIR
#define DT_DIR 4
#endif
#include <time.h>
#define _XOPEN_SOURCE 700
#include <signal.h>
#include <limits.h>
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define PORT 12347  // Port number for server

// Function to handle error messages
void error(const char *msg) {
    perror(msg);
    exit(1);
}

// Updated Directory Entry structure
typedef struct {
    char *name;     // Directory name
    time_t ctime;   // Creation time of the directory
} DirEntry;

// Function to compare directory names alphabetically
int compare_by_name(const void *a, const void *b) {     // Cast void pointers to pointers to char pointers, for directory names
    const char *dirA = *(const char**)a;
    const char *dirB = *(const char**)b;
    return strcmp(dirA, dirB);
}

// Function to compare directories by creation time
int compare_by_time(const void *a, const void *b) {    // Cast void pointers to pointers to time_t, which typically represents time in seconds
    const time_t *timeA = (const time_t*)a;
    const time_t *timeB = (const time_t*)b;
    return (*timeA > *timeB) - (*timeA < *timeB);
}

// Modified list_directories to send output over socket
void list_directories(int sock, const char *dir_path, int sort_by_time) {
    DIR *d;
    struct dirent *entry;
    char output[1024] = "";  // Buffer to accumulate directory names
    char temp[256];
    int count = 0;
    char *names[256];  // Array to store directory names for sorting
    time_t times[256]; // Array to store times for time-based sorting

    if ((d = opendir(dir_path)) == NULL) { // Attempt to open the directory specified by dir_path
        sprintf(output, "Failed to open directory %s\n", dir_path);     // If opening the directory fails, format an error message
        write(sock, output, strlen(output));
        return;
    }
// Read entries from the directory until there are no more
    while ((entry = readdir(d)) != NULL) {
            // Check if the directory entry is a directory and not '.' or '..'
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                    // Duplicate the directory name and save it in the names array
            names[count] = strdup(entry->d_name);
            struct stat info;
            char full_path[1024];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
            stat(full_path, &info);
            times[count] = info.st_ctime;
            count++;
        }
    }
    closedir(d);
// Check if directories should be sorted by time rather than name
    if (sort_by_time) {
        qsort(times, count, sizeof(time_t), compare_by_time); // Sort the array of creation times using the compare_by_time function
        for (int i = 0; i < count; i++) { // After sorting by time, generate a list of directory names in the sorted order
            snprintf(temp, sizeof(temp), "%s\n", names[i]);
            strcat(output, temp);
        }
    } else {  // Sort the array of directory names alphabetically using the compare_by_name function
        qsort(names, count, sizeof(char*), compare_by_name);
        for (int i = 0; i < count; i++) {     // Generate a list of directory names in alphabetical order
            snprintf(temp, sizeof(temp), "%s\n", names[i]);
            strcat(output, temp);
        }
    }

    // Send the sorted output back to the client
    write(sock, output, strlen(output));

    // Free allocated memory
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
}

// Recursive function to search for a file in the directory and its subdirectories
int find_file(const char *basepath, const char *search_filename, char *result_path) {
    DIR *dir;
    struct dirent *entry;
    char path[1024];

    if (!(dir = opendir(basepath)))
        return 0; // Unable to open directory

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue; // Skip the dot and dot-dot directories

        snprintf(path, sizeof(path), "%s/%s", basepath, entry->d_name);

        if (entry->d_type == DT_DIR) {
            // Recursively search in this directory
            if (find_file(path, search_filename, result_path)) {
                closedir(dir);
                return 1; // File found
            }
        } else {
            if (strcmp(entry->d_name, search_filename) == 0) {
                // File found, copy the full path to result
                strncpy(result_path, path, 1024);
                closedir(dir);
                return 1;
            }
        }
    }
    closedir(dir);
    return 0; // File not found
}

// Function to send file information back to the client
void send_file_info(int sock, const char *filename) {
    char full_path[1024];
    char output[2048];
    struct stat statbuf;

    // Start search from the home directory
    if (find_file(getenv("HOME"), filename, full_path)) {
        if (stat(full_path, &statbuf) == 0) {
            // Prepare the output message with file details
            snprintf(output, sizeof(output), "File: %s\nSize: %ld bytes\nCreated: %sPermissions: %o\n",
                     filename, statbuf.st_size, ctime(&statbuf.st_ctime), statbuf.st_mode & 0777);
            write(sock, output, strlen(output));
        } else {
            // Failed to stat the file, even though it was found
            snprintf(output, sizeof(output), "Error accessing file details.\n");
            write(sock, output, strlen(output));
        }
    } else {
        // File not found
        snprintf(output, sizeof(output), "File not found\n");
        write(sock, output, strlen(output));
    }
}

void find_files_by_size(const char *base_path, int size1, int size2, FILE *out) { // Function to find and list files within a specific size range in a directory hierarchy
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;
    char path[1024];

    if (!(dir = opendir(base_path))) // Attempt to open the directory at the given base path

        return;

    while ((entry = readdir(dir)) != NULL) {  // Read each entry in the directory
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        // Build the full path for each file/directory
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
        if (stat(path, &statbuf) == 0) {    // Retrieve information about the file/directory
            if (S_ISDIR(statbuf.st_mode)) {    // If the entry is a directory, recursively search it
                find_files_by_size(path, size1, size2, out);
            } else if (S_ISREG(statbuf.st_mode)) { // If the entry is a regular file
                // Check if the file size is within the specified range
                if (statbuf.st_size >= size1 && statbuf.st_size <= size2) {
                    fprintf(out, "%s\n", path);
                }
            }
        }
    }
    closedir(dir);
}

void send_files_by_size(int sock, int size1, int size2) {
    char temp_file[] = "/tmp/file_list.txt";
    FILE *out = fopen(temp_file, "w");
    if (!out) {
        char* error_msg = "Error: Unable to open temporary file\n";
        write(sock, error_msg, strlen(error_msg));
        return; // Exit if file cannot be opened
    }

    // Find all files within the size range and write their paths to the file
    find_files_by_size(getenv("HOME"), size1, size2, out);
    fclose(out);

    // Create a tar.gz file containing all the files listed in file_list.txt
    char tar_command[256];
    snprintf(tar_command, sizeof(tar_command), "tar -czf /tmp/files.tar.gz -T %s", temp_file);
    system(tar_command);

    // Check if any files were added to the file list
    struct stat statbuf;
    stat(temp_file, &statbuf);
    if (statbuf.st_size == 0) {  // File is empty, no files found
        char* msg = "No file found\n";
        write(sock, msg, strlen(msg));
        unlink(temp_file);
        return;
    }

    // Send the tar.gz file
    FILE *file = fopen("/tmp/files.tar.gz", "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        rewind(file);

        // Allocate buffer to hold file content and EOF marker
        char *buffer = malloc(fsize + 4); // Additional space for EOF marker
        if (!buffer) {
            char* error_msg = "Error: Memory allocation failed\n";
            write(sock, error_msg, strlen(error_msg));
            fclose(file);
            unlink("/tmp/files.tar.gz");
            unlink(temp_file);
            return;
        }

        // Read file content into buffer
        fread(buffer, 1, fsize, file);

        // Append EOF marker at the end of the buffer
        memcpy(buffer + fsize, "EOF", 4);

        // Send file data along with EOF marker
        write(sock, buffer, fsize + 4);

        // Clean up
        free(buffer);
        fclose(file);
    } else {
        char* error_msg = "Error: Unable to open tar.gz file\n";
        write(sock, error_msg, strlen(error_msg));
    }

    // Clean up temporary files
    unlink("/tmp/files.tar.gz");
    unlink(temp_file);
}
// Function to recursively find and list files of specified types within a directory hierarchy
// Function to recursively find and list files of specified types within a directory hierarchy
void find_files_by_type(const char *base_path, const char **types, int num_types, FILE *out) {
    DIR *dir;  // Pointer to the directory
    struct dirent *entry;  // Pointer to each directory entry
    struct stat statbuf;  // Structure to store file information
    char path[1024];  // Buffer to hold the path of each file

    // Try to open the directory specified by base_path
    if (!(dir = opendir(base_path)))
        return;  // Exit the function if the directory cannot be opened

    // Loop through each entry in the directory
    while ((entry = readdir(dir)) != NULL) {
        // Skip the '.' and '..' entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Construct the full path of the current entry
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
        // Attempt to get file attributes
        if (stat(path, &statbuf) == 0) {
            // If the entry is a directory, recursively search it
            if (S_ISDIR(statbuf.st_mode)) {
                find_files_by_type(path, types, num_types, out);
            } else if (S_ISREG(statbuf.st_mode)) {  // If the entry is a regular file
                // Loop through the list of file types we are interested in
                for (int i = 0; i < num_types; i++) {
                    char *ext = strrchr(entry->d_name, '.');  // Find the file extension
                    // Check if the file has an extension and if it matches one of the types specified
                    if (ext && strcmp(ext + 1, types[i]) == 0) {
                        // If a match is found, print the file path to the output file
                        fprintf(out, "%s\n", path);
                        break;  // Exit the loop once a match is found to avoid redundant checks
                    }
                }
            }
        }
    }
    // Close the directory to free resources
    closedir(dir);
}

void send_files_by_type(int sock, char *types_string) {
    const char *types[3];
    int num_types = 0;
    char *token = strtok(types_string, " ");
    while (token != NULL && num_types < 3) {
        types[num_types++] = token;
        token = strtok(NULL, " ");
    }

    char temp_file[] = "/tmp/file_list.txt";
    FILE *out = fopen(temp_file, "w");
    if (!out) {
        char* error_msg = "Error: Unable to open temporary file\n";
        write(sock, error_msg, strlen(error_msg));
        return; // Exit if file cannot be opened
    }

    find_files_by_type(getenv("HOME"), types, num_types, out);
    fclose(out);

    // Create a tar.gz file containing all the files listed in file_list.txt
    char tar_command[256];
    snprintf(tar_command, sizeof(tar_command), "tar -czf /tmp/files.tar.gz -T %s", temp_file);
    system(tar_command);

    // Send the tar.gz file
    FILE *file = fopen("/tmp/files.tar.gz", "rb");
    if (file) {
        fseek(file, 0, SEEK_END);
        long fsize = ftell(file);
        rewind(file);

        // Allocate buffer to hold file content and EOF marker
        char *buffer = malloc(fsize + 4); // Additional space for EOF marker
        if (!buffer) {
            char* error_msg = "Error: Memory allocation failed\n";
            write(sock, error_msg, strlen(error_msg));
            fclose(file);
            unlink("/tmp/files.tar.gz");
            unlink(temp_file);
            return;
        }

        // Read file content into buffer
        fread(buffer, 1, fsize, file);

        // Append EOF marker at the end of the buffer
        memcpy(buffer + fsize, "EOF", 4);

        // Send file data along with EOF marker
        write(sock, buffer, fsize + 4);

        // Clean up
        free(buffer);
        fclose(file);
    } else {
        char* error_msg = "Error: Unable to open tar.gz file\n";
        write(sock, error_msg, strlen(error_msg));
    }

    // Clean up temporary files
    unlink("/tmp/files.tar.gz");
    unlink(temp_file);
}

// Helper function to convert date string to time_t
time_t parse_date(const char *date_str) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    strptime(date_str, "%Y-%m-%d", &tm);
    return mktime(&tm);
}
// Function to list all relevant files into a temporary file
void find_files_by_date(const char *base_path, time_t input_date, FILE *out, int before) {
    DIR *dir = opendir(base_path);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    char path[1024];
    struct stat statbuf;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;  // Skip '.' and '..'
        
        snprintf(path, sizeof(path), "%s/%s", base_path, entry->d_name);
        if (stat(path, &statbuf) == -1) continue;  // Skip if stat fails

        if (S_ISREG(statbuf.st_mode) &&
            ((before && statbuf.st_mtime <= input_date) || (!before && statbuf.st_mtime >= input_date))) {
            fprintf(out, "%s\n", path);
        } else if (S_ISDIR(statbuf.st_mode)) {
            find_files_by_date(path, input_date, out, before);
        }
    }
    closedir(dir);
}

// Execute tar command using fork and exec with improved error handling
int execute_tar(const char *tar_args[]) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
        return -1;
    } else if (pid > 0) {
        // Parent process: waiting for the child to terminate
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            int exit_status = WEXITSTATUS(status);
            printf("tar exited with status %d\n", exit_status);
            if (exit_status != 0) {
                return -1;
            }
        } else if (WIFSIGNALED(status)) {
            printf("tar killed by signal %d\n", WTERMSIG(status));
            return -1;
        }
        return 0;
    } else {
        // Child process: running the tar command
        execvp("tar", (char *const *)tar_args);
        // Note that execvp only returns if an error occurred
        fprintf(stderr, "Execution of tar failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

// Main function for archiving and sending files
void send_files_by_date(int sock, const char *date, int before) {
    char temp_file[] = "/tmp/file_list.txt";
    FILE *out = fopen(temp_file, "w");
    if (!out) {
        perror("Failed to open temporary file");
        return;
    }

    time_t input_date = parse_date(date);
    find_files_by_date(getenv("HOME"), input_date, out, before);
    fclose(out);

    printf("File search completed, checking file list...\n");
    // Check if the file list is empty
    struct stat statbuf;
    if (stat(temp_file, &statbuf) == -1 || statbuf.st_size == 0) {
        printf("No files matched the criteria or failed to write to file list.\n");
        unlink(temp_file);
        return;
    }

    printf("Archiving files...\n");
    const char *tar_args[] = {"tar", "-czf", "/tmp/files.tar.gz", "-T", temp_file, NULL};
    if (execute_tar(tar_args) != 0) {
        perror("Failed to create archive");
        unlink(temp_file);
        return;
    }

    printf("Archive created, preparing to send files...\n");
    int file = open("/tmp/files.tar.gz", O_RDONLY);
    if (file == -1) {
        perror("Failed to open archive for reading");
        unlink(temp_file);
        unlink("/tmp/files.tar.gz");
        return;
    }

    struct stat file_stat;
    if (fstat(file, &file_stat) == -1) {
        perror("Failed to get file size");
        close(file);
        unlink(temp_file);
        unlink("/tmp/files.tar.gz");
        return;
    }

    // Allocate buffer to hold file content and EOF marker
    char *buffer = malloc(file_stat.st_size + 4); // Additional space for EOF marker
    if (!buffer) {
        perror("Memory allocation failed");
        close(file);
        unlink(temp_file);
        unlink("/tmp/files.tar.gz");
        return;
    }

    if (read(file, buffer, file_stat.st_size) != file_stat.st_size) {
        perror("Error reading file");
        free(buffer);
        close(file);
        unlink(temp_file);
        unlink("/tmp/files.tar.gz");
        return;
    }

    // Append EOF marker at the end of the buffer
    memcpy(buffer + file_stat.st_size, "EOF", 4);

    // Send file data along with EOF marker
    if (write(sock, buffer, file_stat.st_size + 4) < 0) {
        perror("Failed to send file");
    }

    free(buffer);
    close(file);

    // Clean up
    unlink(temp_file);
    unlink("/tmp/files.tar.gz");
}

// Public functions that handle sending files before and after a specific date
void send_files_by_date_before(int sock, const char *date) {
    send_files_by_date(sock, date, 1);  // before = 1
}

void send_files_by_date_after(int sock, const char *date) {
    send_files_by_date(sock, date, 0);  // before = 0
}

void crequest(int sock) {
    char buffer[256];
    int n;

    // Enter an infinite loop to handle commands until 'quitc'
    while (1) {
        bzero(buffer, 256);
        n = read(sock, buffer, 255);
        if (n < 0) error("ERROR reading from socket");

        // Remove newline character from the command if present
        buffer[strcspn(buffer, "\n")] = 0;

        if (strcmp(buffer, "quitc") == 0) {
            break; // Exit the loop and terminate child process
        }

        if (strncmp(buffer, "dirlist", 7) == 0) {
            // Parse command for sorting type
            char *sort_type = buffer + 8;
            int sort_by_time = 0; // Default to alphabetical sort

            // Determine the sorting type based on the command suffix
            if (strcmp(sort_type, "-t") == 0) {
                sort_by_time = 1; // Sort by time if '-t' is specified
            } else if (strcmp(sort_type, "-a") == 0) {
                sort_by_time = 0; // Sort alphabetically if '-a' is specified
            } else {
                // Handle error or unrecognized sort type
                char *error_msg = "Unrecognized sorting option. Use '-a' for alphabetical or '-t' for time-based sorting.\n";
                write(sock, error_msg, strlen(error_msg));
                continue;
            }

            // Fetch the directory path (usually the home directory)
            char *dir_path = getenv("HOME"); // Default directory path

            // Call the list_directories function with the path and sort type
            list_directories(sock, dir_path, sort_by_time);
        } else if (strncmp(buffer, "w24fn ", 6) == 0) {
            char* filename = buffer + 6;
            send_file_info(sock, filename);
        } else if (strncmp(buffer, "w24fz ", 6) == 0) {
            // Handle file size range command
            int size1, size2;
            sscanf(buffer + 6, "%d %d", &size1, &size2);
            send_files_by_size(sock, size1, size2);
        } else if (strncmp(buffer, "w24ft ", 6) == 0) {
            // Handle file type command
            char* types = buffer + 6;
            send_files_by_type(sock, types);
        } else if (strncmp(buffer, "w24fdb ", 7) == 0) {
            // Handle files by date before command
            char* date = buffer + 7;
            send_files_by_date_before(sock, date);
        } else if (strncmp(buffer, "w24fda ", 7) == 0) {
            // Handle files by date after command
            char* date = buffer + 7;
            send_files_by_date_after(sock, date);
        } else {
            char* msg = "Invalid command\n";
            write(sock, msg, strlen(msg));
        }
    }

    close(sock); // Close the socket once 'quitc' is received
}

void handle_client(int);

int main(int argc, char *argv[]) {
    int sockfd, newsockfd;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

    // Handle SIGCHLD to prevent child processes from becoming zombies
    signal(SIGCHLD, SIG_IGN);

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    // Initialize server structure with zeros
    bzero((char *) &serv_addr, sizeof(serv_addr));

    // Setup the host_addr structure for use in bind call
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY; // Automatically fill with my IP
    serv_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    // Listen for incoming connections
    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (newsockfd < 0)
            error("ERROR on accept");

        int pid = fork();
        if (pid < 0)
            error("ERROR on fork");

        if (pid == 0) {  // This is the child process
            close(sockfd);
            // Function to handle communication with the client
            handle_client(newsockfd);
            exit(0);
        } else {
            close(newsockfd);  // Parent doesn't need this socket
        }
    }
    
    // This line will typically never be reached
    close(sockfd);
    return 0;
}

void handle_client(int sock) {
    crequest(sock);  // Function previously discussed that handles client requests
}