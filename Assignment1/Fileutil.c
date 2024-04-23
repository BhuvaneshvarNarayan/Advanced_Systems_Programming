/*
Author: Bhuvaneshvar Narayan J H
Subject: Advanced Systems Programming
University of Windsor
Assignment 1
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Accessing Operating system functionality
#include <sys/stat.h> // For file status information
#include <sys/types.h> // For system data types
#include <ftw.h> //  For nftw function
#include <libgen.h> // For filename manipulation functions

// Global variables to store the arguments and options
char *filename, *storage_dir = NULL, *extension = NULL;
int option = 0; // Initialize option to 0
char files[1024] = ""; // Declare the files variable as global

// Function to check if a file has a given extension
int has_extension(const char *file, const char *ext) {
  char *dot = strrchr(file, '.'); // Find the last dot in the file name
  if (!dot || dot == file) return 0; // No extension
  return strcmp(dot + 1, ext) == 0; // Compare the extension to see if it matches
}

// Function block to copy a file from source to destination
void copy_file(const char *src, const char *dest) {
  FILE *fsrc, *fdest;
  char buffer[1024];// Temporary storage to store the file while it is being copied
  int bytes; // To store the bytes read in each iteration 

  fsrc = fopen(src, "r"); // Open the source file for reading
  if (fsrc == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  fdest = fopen(dest, "w"); // Open the destination file for writing
  if (fdest == NULL) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  while ((bytes = fread(buffer, 1, sizeof(buffer), fsrc)) > 0) { // Loop will run until there is nothing to read
    // This reads from the source file and writes to the destination file
    fwrite(buffer, 1, bytes, fdest);
  }

  fclose(fsrc); // Close the source file
  fclose(fdest); // Close the destination file
}

// Function block to move a file from source to destination
void move_file(const char *src, const char *dest) {
  copy_file(src, dest); // Copy the file
  remove(src); // Remove the source file
}

// Function block to create a tar file from a list of files
void create_tar(const char *tar, const char *files) {
  // Calculate the size needed for the command string
    size_t command_size = strlen("tar -cf ") + strlen(tar) + 1 + strlen(files) + 1;

    // Allocate memory for the command string
    char *command = (char *)malloc(command_size);
    if (command == NULL) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Construct the tar command
    snprintf(command, command_size, "tar -cf %s %s", tar, files);

    // Execute the command
    system(command);

    // Free the allocated memory
    free(command);
}

// Function to process each file or directory visited by nftw
int process(const char *path, const struct stat *st, int flag, struct FTW *ftw) {
  char dest[1024];  //char dest of size [1024] to store the destination path for the file moving or copying
  if (flag == FTW_F) { // If the path specified is a file
    if (filename && strcmp(basename(path), filename) == 0) { // This block checks if filename passed as an argument matches with the basename of the current path
      printf("%s\n", path); // Print the absolute path of the file
      if (option == 1 || option == 2) { // These are the options for -cp or -mv
        sprintf(dest, "%s/%s", storage_dir, filename); // Construct the destination path using the storage directory and filename
        if (option == 1) { // If the option is -cp
          copy_file(path, dest); // Copy the file
        } else { // If the option is -mv
          move_file(path, dest); // Move the file
        }
        printf("File %s to the storage_dir\n", option == 1 ? "copied" : "moved"); // Print the result
      }
      return 1; // Return 1 to stop the traversal after the first successful search
    } else if (extension) { // If the extension argument is given and the file has that extension
      if (strlen(files) + strlen(path) + 1 < sizeof(files)) {
        printf("%s\n",path);
        strcat(files, path); // Append the path to the list of files
        strcat(files, " "); // Add a space as a separator
      } else {
        fprintf(stderr, "Error: Insufficient buffer for file paths\n");
        exit(EXIT_FAILURE);
      }
      printf("%s\n", path); // Print the absolute path of the file
    }
  }
  return 0; // Return 0 to continue the traversal
}

int main(int argc, char *argv[]) {
  char root_dir[1024], tar[1024];
  struct stat st; // Rename the variable to st used to store file system status info

  if (argc < 3 || argc > 5) { // If the number of arguments is less than 3 or greater than 5
    fprintf(stderr, "You have entered incorrect number of arguments. Here is how you can give:");
    fprintf(stderr, "Usage: fileutil [root_dir] filename\n");
    fprintf(stderr, "Or: fileutil [root_dir] [storage_dir] [options] filename\n");
    fprintf(stderr, "Or: fileutil [root_dir] [storage_dir] extension\n");
    exit(EXIT_FAILURE);
  }

  strcpy(root_dir, argv[1]); // Copy the root_dir argument
  if (stat(root_dir, &st) != 0 || !S_ISDIR(st.st_mode)) { // Use the renamed variable to check if the root directory is valid
    fprintf(stderr, "You have entered the Invalid root_directory\n");
    exit(EXIT_FAILURE);
  }

  if (argc == 3) { // If the number of arguments is 3
    filename = argv[2]; // Set the filename as the second argument
    option = 0; // Set the option to 0 (no option)
    extension = NULL; // Set the extension to NULL
  } else if (argc == 5) { // If the number of arguments is 5
    storage_dir = argv[2]; // Set the storage_dir argument
    if (stat(storage_dir, &st) != 0 || !S_ISDIR(st.st_mode)) { // Use the renamed variable to check if the storage directory is valid
      fprintf(stderr, "You have entered the Invalid storage_directory\n");
      exit(EXIT_FAILURE);
    }
    if (strcmp(argv[3], "-cp") == 0) { // If the option is -cp
      option = 1; // Set the option to 1
    } else if (strcmp(argv[3], "-mv") == 0) { // If the option is -mv
      option = 2; // Set the option to 2
    } else { // If the option is invalid
      fprintf(stderr, "You have entered an Invalid option. Please try either -cp or -mv\n");
      exit(EXIT_FAILURE);
    }
    filename = argv[4]; // Set the filename argument
    extension = NULL; // Set the extension to NULL
  } else if (argc == 4) { // If the number of arguments is 4 For Tar Creation
    storage_dir = argv[2]; // Set the storage_dir argument
    if (stat(storage_dir, &st) != 0) { // Use the renamed variable
      mkdir(storage_dir, 0755); // Create the storage_dir
    }
    option = 0; // Set the option to 0 (no option)
    filename = NULL; // Set the filename to NULL
    extension = argv[3]; // Set the extension argument
    sprintf(tar, "%s/a1.tar", storage_dir); // Construct the tar file name
  }  else { // If the number of arguments is invalid
    fprintf(stderr, "Invalid number of arguments. Please try again with either 3 to 5 arguments\n");
    exit(EXIT_FAILURE);
  }
    if(nftw(root_dir, process, 20, FTW_PHYS) ==0) {// Use FTW_PHYS to avoid following symbolic links
        if (extension) { // If the extension argument is given
            create_tar(tar, files); // Create the tar file from the list of files
            printf("The Tar file has been created in the storage_directory\n"); // Print the result
        } else { // If the filename argument is given
            printf("Your Search is Unsuccessful. Please try again by giving the correct path\n"); // Print the result
        }
    }
    return 0;
}