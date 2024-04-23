/*
Author: Bhuvaneshvar Narayan J H
Subject: Advanced Systems Programming
University of Windsor
Assignment 3
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <pwd.h>
// Define constants for maximum command arguments, command limit, and input buffer size
#define MAX_ARGS 6
#define COMMANDS_LIMIT 7
#define INPUT_BUFFER_SIZE 256
// Global variables
char *cmd_args[MAX_ARGS]; // Command arguments
int arg_count; // Number of arguments
pid_t last_bg_pid = -1; // Global variable to store the last background process ID
// Function prototype for executing a command
void execute_command(int input_fd, int output_fd, int background, int *exit_status);
// Function to execute a new shell instance in a separate terminal window
void execute_new_shell_instance() {
    // Fork the current process to create a child process
    pid_t pid = fork();
    if (pid == 0) {
        // Child process: execute 'xterm' to open a new terminal window and run "./shell24"
        execlp("xterm", "xterm", "-e", "./shell24", (char *)NULL);
        // If execlp returns, it indicates a failure to launch 'xterm'
        perror("Failed to launch new shell instance!");
        exit(EXIT_FAILURE); // Exit with a failure status
    } else if (pid < 0) {
        perror("Failed to fork"); // Fork failed: unable to create a child process
    } else {
        // Parent process: wait for the new shell to launch
         waitpid(pid, NULL, 0); 
    }
}
// Function to expand a path starting with '~' to an absolute path
char* expand_path(char* path) {
    // Check if the path starts with '~' which indicates home directory
    if (path[0] == '~') {
        char* home_dir; // Variable to store the home directory path
        char* remaining_path; // Variable to store the part of the path after '~'
        // Check if '~' is followed by '/' (root directory) or end of string (only '~')
        if (path[1] == '/' || path[1] == '\0') {
            home_dir = getenv("HOME"); // For the current user, get the HOME environment variable as the home directory
            remaining_path = &path[1]; // The remaining path is everything after '~'
        } else {
            // If specific user's home is referenced (~username), find the end of the username
            char* end_of_user = strchr(path, '/');
            if (end_of_user == NULL) end_of_user = strchr(path, '\0');
            size_t user_len = end_of_user - &path[1]; // Calculate the length of the username
            char user[user_len + 1]; // Allocate space for username plus null terminator
            strncpy(user, &path[1], user_len); // Copy the username into the user variable
            user[user_len] = '\0'; // Null-terminate the username string
            // Get the user's home directory using the username
            struct passwd* pw = getpwnam(user);
            if (pw == NULL) {
                // If the user doesn't exist, print an error and return NULL
                fprintf(stderr, "User %s not found\n", user);
                return NULL;
            } // Set the home directory and remaining path
            home_dir = pw->pw_dir;
            remaining_path = end_of_user;
        } // Calculate the lengths of the home directory and the remaining path
        size_t home_len = strlen(home_dir);
        size_t path_len = strlen(remaining_path);
        // Allocate memory for the full path
        char* full_path = malloc(home_len + path_len + 1);
        if (full_path == NULL) {
            // If memory allocation fails, print an error and exit
            fprintf(stderr, "Memory allocation failed\n");
            exit(EXIT_FAILURE); 
        }
        // Construct the full path
        strcpy(full_path, home_dir);
        strcat(full_path, remaining_path);
        return full_path;  // Return the expanded path
    } // If the path does not start with '~', return it unchanged
    return path;
}
// Function to concatenate and print the contents of multiple files to standard output
void concatenate_files(char **files, int file_count) {
    // Loop through each file specified in the 'files' array
    for (int i = 0; i < file_count; ++i) {
        // Attempt to open the current file for reading only
        int fd = open(files[i], O_RDONLY);
        // Check if the file was successfully opened
        if (fd < 0) {
            perror("Error opening file"); // If not, print an error message and continue to the next file
            printf("Failed to open %s\n", files[i]);
            continue; // Skip the rest of the loop for this iteration
        }
         // Buffer to hold file content read from the file
        char buffer[1024];
        ssize_t bytes_read; // Variable to hold the number of bytes read
         // Read the file content in chunks of up to 1024 bytes
        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            write(STDOUT_FILENO, buffer, bytes_read); // Write the read content to standard output
        }
        close(fd); // Closing the file descriptor as we're done with this file
    }
    printf("\n"); // Add a newline after printing the concatenated files' content
}
// Function to bring the last background process to the foreground and wait for it to complete
void bring_to_foreground() {
    if (last_bg_pid > 0) { // Check if there is a last background process ID recorded
        int status; // Variable to store the exit status of the waited-for process
        printf("Bringing process [%d] to foreground.\n", last_bg_pid); // Inform the user which process is being brought to the foreground
        waitpid(last_bg_pid, &status, 0); // Wait for the specified process to complete
        last_bg_pid = -1; // Reset the last_bg_pid since it's no longer in the background
    } else {
        printf("No background process to bring to foreground.\n"); // If there is no background process to bring to the foreground, inform the user
    }
}
// Function to handle and execute the command entered by the user
void handle_command(char *input) {
    char *token; // Pointer to hold the current token being processed
    int bg_process = 0; // Flag to indicate if the current command should run in the background
    char *file_tokens[MAX_ARGS]; // Array to store filenames when in concatenation mode
    int file_count = 0; // Counter for the number of files in concatenation mode
    int concat_mode = 0; // Flag to indicate if the shell is in concatenation mode
    int input_fd = STDIN_FILENO, output_fd = STDOUT_FILENO; // File descriptors for input and output redirection
    int pipe_fd[2], pipe_count = 0, prev_status = 0; // Variables for handling piping
    int execute = 1; // Flag to determine if the command should be executed

    arg_count = 0; // Initialize argument counter
    token = strtok(input, " \t\n"); // Tokenize the input string on spaces, tabs, and newlines
    // Check if the command is 'exit'
    if (token != NULL && strcmp(token, "exit") == 0) {
        printf("Exiting shell24.\n");
        exit(0); // Exit the program
    }
    char *prev_token = NULL; // Keep track of the previous token for special handling
    // Loop through each token in the input string
    while (token != NULL) {
        if (arg_count >= MAX_ARGS) { // Check for exceeding the maximum number of arguments
            printf("Exceeded maximum number of arguments.\n");
            return;
        }
    // Special handling for the 'newt' command (to open a new terminal instance)
        if (strcmp(token, "newt") == 0 && arg_count == 0) {
            execute_new_shell_instance();
            return;
        } 
        // Checking and Entering concatenation mode
        if (strcmp(token, "#") == 0) {
            if (!concat_mode && prev_token != NULL) {
            // If entering concat mode and there's a token before '#'
            concat_mode = 1;
            file_tokens[file_count++] = prev_token; // Add the token before '#' to file_tokens
            }
            concat_mode = 1; // Mark the start of concatenation mode
            token = strtok(NULL, " \t\n"); // Move to the next token
            continue; // Skip further processing in this iteration
        }
        // Handle bringing process to foreground
        if (strcmp(token, "fg") == 0 && arg_count == 0) {
            bring_to_foreground();
            return; // Return early since no further action is needed for fg command
        }
        // Collecting file names for concatenation
        if (concat_mode) {
            if (file_count < MAX_ARGS) { // Check if the number of files does not exceed the maximum allowed
                file_tokens[file_count++] = token; // Store the current token as a file name for later concatenation
            } else {
                printf("You have exceeded the maximum number of files for concatenation.\n"); // Exceeding the maximum number of files allowed for concatenation
                return;
            }
        } else { 
            // Process shell command features: background processing, command chaining, redirection, and piping
            if (strcmp(token, "&") == 0) {
                bg_process = 1; // Set background process flag and exit loop to execute the command in the background
                break;
            } else if (strcmp(token, ";") == 0) { // End of command; execute it and reset for the next command
                cmd_args[arg_count] = NULL; // Null-terminate the argument list
                if (execute) execute_command(input_fd, output_fd, bg_process, &prev_status);
                arg_count = 0; // Reset argument count for the next command
                execute = 1; // Reset execution flag
                // Conditional execution based on the success or failure of the previous command
            } else if (!strcmp(token, "&&") || !strcmp(token, "||")) { 
                cmd_args[arg_count] = NULL; // Null-terminate the argument list
                if (execute) execute_command(input_fd, output_fd, bg_process, &prev_status);
                arg_count = 0; // Reset argument count for the next command
                execute = (!strcmp(token, "&&") && prev_status == 0) || (!strcmp(token, "||") && prev_status != 0);
            }
            // Piping between commands 
            else if (strcmp(token, "|") == 0) { 
                if (++pipe_count > COMMANDS_LIMIT) {
                    printf("Too many pipes.\n");
                    return;
                }
                cmd_args[arg_count] = NULL;
                if (pipe(pipe_fd) < 0) {
                    perror("Pipe failed");
                    exit(EXIT_FAILURE);
                }
                execute_command(input_fd, pipe_fd[1], bg_process, &prev_status);
                close(pipe_fd[1]); // Close the write end of the pipe in the current command
                input_fd = pipe_fd[0]; // Use the read end of the pipe as input for the next command
                arg_count = 0; // Reset argument count for the next command
            } 
            // Output redirection
            else if (!strcmp(token, ">") || !strcmp(token, ">>")) { 
                char* redirection_type = token; // Save the redirection type before fetching the filename
                token = strtok(NULL, " \t\n"); // This token is now the filename
                if (!token) {
                    printf("Missing file name.\n");
                    return;
                }
                // Now correctly set flags based on the saved redirection type
                int flags = (!strcmp(redirection_type, ">>")) ? (O_WRONLY | O_CREAT | O_APPEND) : (O_WRONLY | O_CREAT | O_TRUNC);
                output_fd = open(token, flags, 0644); // Open the file for redirection
                if (output_fd < 0) {
                    perror("File open error");
                    return;
                }
                cmd_args[arg_count] = NULL;
                execute_command(input_fd, output_fd, bg_process, &prev_status);
                close(output_fd); // Close the output file descriptor
                output_fd = STDOUT_FILENO; // Reset the output FD to stdout
                arg_count = 0; // Reset argument count for the next command
            } else if (!strcmp(token, "<")) { // Input redirection
                token = strtok(NULL, " \t\n"); // Get the filename for input redirection
                if (!token) {
                    printf("Missing file name.\n");
                    return;
                }
                input_fd = open(token, O_RDONLY); // Open the file for input redirection
                if (input_fd < 0) {
                    perror("File open error");
                    return;
                }
            } else { // Standard command argument
                cmd_args[arg_count++] = expand_path(token); // Store the argument, expanding any path as necessary
            }
        }
        prev_token = token; // Updating the previous token for next iteration
        token = strtok(NULL, " \t\n"); // Move to the next token
    }
    // After processing all tokens
    if (concat_mode && file_count > 0) {
        concatenate_files(file_tokens, file_count); // If in concatenation mode with at least one file, concatenate and print the file contents 
    } else if (!concat_mode && execute && arg_count > 0) { // If not in concatenation mode, but have commands to execute, do so
        cmd_args[arg_count] = NULL; // Terminate the argument list
        execute_command(input_fd, output_fd, bg_process, &prev_status);
    }
    // Reset states for next command
    bg_process = 0;
    file_count = 0;
    concat_mode = 0;
    input_fd = STDIN_FILENO;
    output_fd = STDOUT_FILENO;
    pipe_count = 0;
    execute = 1;
    arg_count = 0;
}
// Function to execute a command, potentially with input/output redirection and background execution
void execute_command(int input_fd, int output_fd, int background, int *exit_status) {
    pid_t pid = fork(); // Create a new process
    if (pid == 0) {
        // Child process: If a new input file descriptor is specified, replace the standard input with it
        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO); // Duplicate the input_fd to STDIN_FILENO
            close(input_fd); // Close the original file descriptor as it's no longer needed
        } 
        // If a new output file descriptor is specified, replace the standard output with it
        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO); // Duplicate the output_fd to STDOUT_FILENO
            close(output_fd); // Close the original file descriptor as it's no longer needed
        }  
        // Replace the current process image with the new process specified by cmd_args
        execvp(cmd_args[0], cmd_args);
        perror("Exec failed"); // If fork fails, print an error message
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        // Fork failed
        perror("Fork failed");
    } else {
        // Parent process
        if (background) { // When in background
            last_bg_pid = pid; // Update the last_bg_pid with the new background process's PID
            printf("Process [%d] running in background.\n", pid); // Print the message for background process
        } else {
            // Only wait for the process if it's not a background process
            waitpid(pid, exit_status, 0);
        }
    }
}
// Entry point of the program, Main
int main() {
    // Buffer to store the command input by the user
    char input[INPUT_BUFFER_SIZE];
     // Infinite loop to keep the shell running
    while (1) {
        // Display the shell prompt
        printf("shell24$ ");
        if (!fgets(input, sizeof(input), stdin)) break;  // Read a line of input from the user. If reading fails (e.g., on EOF), exit the loop
        // Checking if the input is more than just a newline character and on success it will pass the input to be handled (parsed and executed)
        if (strlen(input) > 1) handle_command(input); 
    }
    return 0;
}