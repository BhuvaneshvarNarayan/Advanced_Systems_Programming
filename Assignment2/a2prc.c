/*
Author: Bhuvaneshvar Narayan J H
Subject: Advanced Systems Programming
University of Windsor
Assignment 2
*/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h> // Signal handling functions
#include <unistd.h>
#include <sys/types.h> // Data types used in system calls and various other definitions
#include <sys/wait.h> // Declarations for many types of operations on process IDs
#include <sys/stat.h> // Data structures and symbolic constants defining file characteristics
#include <string.h> 
#include <dirent.h> // Directory handling functions
#include <errno.h> // Error number variable
// Function to check if a process is a descendant of another process or not
int isDescendant(pid_t process_id, pid_t root_process) {
    char proc_file[100]; // Buffer to store the path to the /proc file
    pid_t ppid; // Variable to store the parent process ID

    // Construct the path to the /proc file for the given process ID
    sprintf(proc_file, "/proc/%d/stat", process_id);

    // Open the /proc file for the process
    FILE *fp = fopen(proc_file, "r");
    if (fp == NULL) {
        perror("Error opening /proc file"); // Printing an error message if the file isn't opening
        return 0;
    }

    // Read the parent process ID from the /proc file
    fscanf(fp, "%*d %*s %*c %d", &ppid);
    fclose(fp); // Closing the file after reading

    // Iterate until reaching the root process or init process (PID 1)
    while (ppid != root_process) {
        if (ppid == 1) {
            return 0;  // If the parent process ID is 1 (init process), it's not a valid ancestor
        }

        // Get information about the parent process
        sprintf(proc_file, "/proc/%d/stat", ppid);
        fp = fopen(proc_file, "r");
        if (fp == NULL) {
            perror("Error opening /proc file"); // Printing an error message if the file cannot be opened for error handling
            return 0;
        }
        // Read the parent process ID from the /proc file
        fscanf(fp, "%*d %*s %*c %d", &ppid);
        fclose(fp); // Closing the file after reading
    }

    return 1;  // If the function reaches here, it means the process is a descendant of the root process
}

// Function to list the descendants of a process
void listDescendants(pid_t process_id, int direct, int defunct, int* found) {
    char cmd[100]; // Buffer to store the command
    sprintf(cmd, "pgrep -P %d", process_id); // Construct the command to find processes with the given process ID as parent
    FILE *fp = popen(cmd, "r"); // Open a pipe to execute the command and read its output
    if (fp != NULL) {
        char buffer[256]; // Buffer to store the output of the command
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
            pid_t pid = atoi(buffer); // Converting the string to a process ID
            *found = 1;  // Set found to true if any descendant is found
            // Check if the process is defunct if the defunct flag is set
            if (!defunct || waitpid(pid, NULL, WNOHANG) == 0) {
                printf("%d\n", pid); // Prints the processID
                // If direct is set, list only immediate descendants
                if (!direct) {
                    listDescendants(pid, 0, defunct, found); // Recursively list descendants of the current process
                }
            }
        }

        pclose(fp); // Close the pipe after reading the output of the command
    }
}

// Function to kill a process
void killProcess(pid_t process_id) {
    if (kill(process_id, SIGKILL) == 0) { // Attempting to kill the process using the SIGKILL signal
        printf("Process with PID %d killed successfully.\n", process_id);
    } else {
        perror("Error killing process"); // If there's an error during the kill operation, it will print an error message
    }
}

// Function to list non-direct descendants
void listNonDirectDescendants(pid_t process_id) {
    int found = 0;  // Flag to track if any descendants are found
    listDescendants(process_id, 0, 0, &found);  // Calling the listDescendants function with appropriate parameters to list non-direct descendants
    // Checking if any non-direct descendants were found
    if (!found) {
        printf("No non-direct descendants\n"); // If no non-direct descendants were found, it will print a message
    }
}

// Function to list immediate descendants
void listImmediateDescendants(pid_t process_id) {
    int found = 0;  // Variable to track if any immediate descendants are found
    // Calling the listDescendants function with appropriate parameters to list immediate descendants setting the direct flag to 1
    listDescendants(process_id, 1, 0, &found); 
    if (!found) { // Checking if any immediate descendants were found
        printf("No immediate descendants\n"); // If no immediate descendants were found, it will print a message
    }
}

void listSiblingProcesses(pid_t process_id, pid_t root_process) {
    char proc_file[100];
    pid_t parent_pid;
    int found = 0;  // Variable to track if any siblings are found
 
    // Fetch the parent PID of 'process_id'
    sprintf(proc_file, "/proc/%d/stat", process_id);
    FILE *fp = fopen(proc_file, "r");
    if (fp == NULL) {
        perror("Error opening /proc file");
        return; // Return if there's an error opening the /proc file
    }
 
    // Read the parent PID from the file
    fscanf(fp, "%*d %*s %*c %d", &parent_pid);
    fclose(fp);
 
    // Now, list all processes to find siblings
    DIR *d;
    struct dirent *dir;
    struct stat statbuf;  // This is for checking file types
 
    d = opendir("/proc");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            // Construct the full path for each directory entry
            char filepath[256];
            sprintf(filepath, "/proc/%s", dir->d_name);
    
            // Get file status
            if (stat(filepath, &statbuf) == -1) {
                // Unable to get stat; skip this entry
                continue;
            }
    
            // Check if the directory entry is a directory
            if (S_ISDIR(statbuf.st_mode)) {
                pid_t pid = atoi(dir->d_name);
                if (pid > 0 && pid != process_id) {  // Ensure it's a valid PID and not the queried process itself
                    // Fetch the parent PID of this process
                    char proc_file[100];
                    sprintf(proc_file, "/proc/%d/stat", pid);
                    FILE *fp = fopen(proc_file, "r");
                    if (fp != NULL) {
                        pid_t this_parent_pid;
                        fscanf(fp, "%*d %*s %*c %d", &this_parent_pid);
                        fclose(fp);
    
                        // Check if this process shares the same parent
                        if (this_parent_pid == parent_pid) {
                            printf("%d\n", pid);
                            found = 1;
                        }
                    }
                }
            }
        }
        closedir(d);
    }
    // Checking if no siblings are found and it will print a message
    if (!found) {
        printf("No sibling/s\n");
    }
}

// Function to pause a process
void pauseProcess(pid_t process_id, pid_t root_process) {
    // Check if the process is a descendant of the root process or is the root process itself
    if (isDescendant(process_id, root_process) || process_id == root_process) {
        kill(process_id, SIGSTOP); // Pausing the specific process using SIGSTOP
        sleep(1);  // Keeping one second delay to ensure that the process is paused
        printf("Process with PID %d paused successfully.\n", process_id);
    } else {
        printf("Does not belong to the process tree\n"); // Print an error message if not a valid process
    }
}

// Function to resume a specific process by PID
void resumeProcess(pid_t process_id, pid_t root_process) {
    if (isDescendant(process_id, root_process) || process_id == root_process) { // Checking if the process is a descendant of the root process or is the root process itself
        // Resume the specific process using SIGCONT signal
        if (kill(process_id, SIGCONT) == 0) {
            printf("Resumed process with PID %d.\n", process_id); // Printing the success message
        } else {
            perror("Error resuming process"); // Printing an error message if there is an issue with resuming the process
        } 
    } else {
        printf("Does not belong to the process tree\n"); // Print an error message if not a valid process
    }
}

// Function to list defunct(Zombie processes) descendants
void listDefunctDescendants(pid_t process_id) {
    int found = 0;  // Variable to track if any defunct descendants are found
    if (isDescendant(process_id, getppid())) { // Checking if the process is a descendant of the parent process
        listDescendants(process_id, 0, 1, &found); // Call the listDescendants function to recursively list defunct descendants setting the defunct argument to 1
    } else {
        printf("Does not belong to the process tree\n"); // Printing an error message if the process is not a valid part of the tree
    }
    // If no defunct descendants are found, it prints a message
    if (!found) {
        printf("No defunct descendants\n");
    }
}

// Function to list grandchildren
void listGrandchildren(pid_t process_id, pid_t root_process) {
    // Check if process_id is a descendant of root_process, not just the parent
    if (isDescendant(process_id, root_process)) {
        int found = 0;  // Variable to track if any grandchildren are found

        char cmd[256]; // Command to find all children (direct descendants) of the given process_id using pgrep
        sprintf(cmd, "pgrep -P %d", process_id);
        FILE *fp = popen(cmd, "r");  // Opening a pipe to run the command and read the output
        if (fp != NULL) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), fp) != NULL) { // Iterate through each line of the command output (each child process)
                pid_t pid = atoi(buffer); // Converting the process ID from string to integer
                // Now, for each child, this will list its children
                listDescendants(pid, 1, 0, &found);
            }
            pclose(fp); // Closing the pipe after reading the output
        }
 
        if (!found) {
            printf("No grandchildren\n"); // if no grandchildren are found, it will print a corresponding message
        }
    } else {
        printf("Does not belong to the process tree\n"); // Print an error message if process_id is not a valid part of the tree
    }
}

// Function to print the status of a process (Defunct or Not a Defunct)
void printProcessStatus(pid_t process_id) {
    char path[40], line[100], *p;
    FILE* statusf;
    // Constructing the path to the status file for the given process ID in the /proc directory
    snprintf(path, 40, "/proc/%d/status", process_id);
    statusf = fopen(path, "r"); // Opening the status file for the process in read mode
    if (!statusf) {
        printf("Could not open status file for process %d.\n", process_id); // Printing an error message if the status file cannot be opened
        return;
    }
    // Iterating through each line of the status file to find the process state information
    while (fgets(line, 100, statusf)) {
        if (strncmp(line, "State:", 6) != 0) // Check if the line contains process state information (identifiable by "State:")
            continue;
        // Line contains process state; extracting the state information
        p = line + 7;
        while (*p == ' ') p++;  // Skip leading spaces
        // Printing the process ID and its state
        printf("Process %d is ", process_id);
        if (*p == 'Z') { // Check if the state is "Z" indicating a defunct (zombie) process
            printf("defunct (zombie).\n");
        } else {
            printf("not defunct.\n");
        } // Close the status file and return after printing the information
        fclose(statusf);
        return;
    }
    // If no state information is found, it will print an error message
    printf("Could not find state information for process %d.\n", process_id);
    fclose(statusf); // Closing the status file
}

// Main Function
int main(int argc, char *argv[]) {
    // Checking if the correct number of arguments is provided
    if (argc < 4) {
        fprintf(stderr, "Usage: %s [process_id] [root_process] [OPTION]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Convert command-line arguments to process IDs and option
    pid_t process_id = atoi(argv[1]);
    pid_t root_process = atoi(argv[2]);
    char *option = argv[3];

    // Check if the process belongs to the process tree
    if (isDescendant(process_id, root_process)) {
        // Perform the specified action based on the provided option
        if (strcmp(option, "-rp") == 0) {  // To kill the Process_ID if it belongs to the give root
            killProcess(process_id);
        } else if (strcmp(option, "-pr") == 0) {  // To kill the root process
            killProcess(root_process);
        } else if (strcmp(option, "-xn") == 0) {  // To list the PIDs of all the non-direct descendants of Process_ID
            listNonDirectDescendants(process_id);
        } else if (strcmp(option, "-xd") == 0) {  // To list the PIDs of all the immediate descendants of Process_ID
            listImmediateDescendants(process_id);
        } else if (strcmp(option, "-xs") == 0) {  // To list the PIDs of all the sibling processes of Process_ID
            listSiblingProcesses(process_id, root_process);
        } else if (strcmp(option, "-xt") == 0) {  // The Process_ID is paused with SIGSTOP
            pauseProcess(process_id, root_process);
        } else if (strcmp(option, "-xc") == 0) {  // SIGCONT is sent to all processes that have been paused earlier
            resumeProcess(process_id, root_process);
        } else if (strcmp(option, "-xz") == 0) {  // To list the PIDs of all descendents of Process_ID that are defunct
            listDefunctDescendants(process_id);
        } else if (strcmp(option, "-xg") == 0) {  // To list the PIDs of all the grandchildren of Process_ID
            listGrandchildren(process_id, root_process);
        } else if (strcmp(option, "-zs") == 0) {  // To print the status of Process_ID is a Zombie or not (Defunct/ Not Defunct)
            printProcessStatus(process_id);
        } else {
            fprintf(stderr, "Invalid option: %s\n Please choose: \n -rp \n -pr \n -xn \n -xd \n -xs \n -xt \n -xc \n -xz \n -xg \n -zs", option); // Errors out and prints a message
            exit(EXIT_FAILURE); // Errors out and prints a message if an invalid option is provided
        }
    } else {
        printf("Does not belong to the process tree\n");
    }
    // Exit the program with success status
    exit(EXIT_SUCCESS);
}