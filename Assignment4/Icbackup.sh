#!/bin/bash

#Author: Bhuvaneshvar Narayan J H
#Subject: Advanced Systems Programming
#University of Windsor
#Assignment 4

# Global variables
# Define the root directory to backup and the backup destination.
BACKUP_ROOT="/home/bhuvaneshvarnarayan"
BACKUP_DIR="$HOME/backup"
LOG_FILE="$BACKUP_DIR/backup.log" # Path to the log file where operations will be logged.
SLEEP_INTERVAL=120 # Interval between backup steps, in seconds.

# Create backup directories for complete, incremental, and differential backups if they do not exist
mkdir -p "$BACKUP_DIR/cbw24" "$BACKUP_DIR/ib24" "$BACKUP_DIR/db24"

# Function to log messages to backup.log
log_message() {
  local message="$1" # The message to log.
  # Logs the message with a timestamp to the log file.
  printf "%s %s\n" "$(date '+%a %d %b %Y %I:%M:%S %p %Z')" "$message" >> "$LOG_FILE"
}

# Function to create a complete backup
create_complete_backup() {
  local timestamp=$(date '+%Y%m%d%H%M%S') # Generate a timestamp for the backup file name.
  local filename="cbw24-$timestamp.tar" # The backup file name for complete backup.
  tar -cvf "$BACKUP_DIR/cbw24/$filename" "$BACKUP_ROOT" --exclude="$BACKUP_DIR" > /dev/null 2>&1
  log_message "$filename was created" # Log the creation of the backup file.
}

# Function to create an incremental backup
create_incremental_backup() {
  local step="$1" # The current backup step.
  local prev_step="$2" # The previous backup step.
  local timestamp=$(date '+%Y%m%d%H%M%S') # Generate a timestamp for the backup file name.
  local filename="ib24-$timestamp.tar" # The backup file name for incremental backup.
  local snapshot_file="$BACKUP_DIR/snapshot_${prev_step}_step.snar" # The previous snapshot file for incremental backup.
  local new_snapshot_file="$BACKUP_DIR/snapshot_${step}_step.snar" # The new snapshot file for the current backup.
  # Copy the previous snapshot to continue the incremental backup.
  cp "$snapshot_file" "$new_snapshot_file" 2>/dev/null || true
  # Creates an incremental tar archive using the new snapshot file.
  tar --listed-incremental="$new_snapshot_file" -cvf "$BACKUP_DIR/ib24/$filename" "$BACKUP_ROOT" --exclude="$BACKUP_DIR" > /dev/null 2>&1
  if [[ -s "$BACKUP_DIR/ib24/$filename" ]]; then  # Check if the backup file is not empty (changes exist).
    log_message "$filename was created" # Log the creation of the backup file.
  else
    log_message "No changes-Incremental backup was not created" # Log no changes were detected.
    rm "$BACKUP_DIR/ib24/$filename" # Remove the empty backup file.
  fi
}

# Function to create a differential backup
create_differential_backup() {
  local timestamp=$(date '+%Y%m%d%H%M%S') # Generate a timestamp for the backup file name.
  local filename="db24-$timestamp.tar" # The backup file name for differential backup.
  # Creates a differential tar archive using the snapshot file from the first step for all changes since then.
  tar --listed-incremental="$BACKUP_DIR/snapshot_1_step.snar" -cvf "$BACKUP_DIR/db24/$filename" "$BACKUP_ROOT" --exclude="$BACKUP_DIR" > /dev/null 2>&1
  if [[ -s "$BACKUP_DIR/db24/$filename" ]]; then # Check if the backup file is not empty (changes exist).
    log_message "$filename was created" # Log the creation of the backup file.
  else
    log_message "No changes-Differential backup was not created" # Log no changes were detected.
    rm "$BACKUP_DIR/db24/$filename" # Remove the empty backup file.
  fi
}

# Main function
main() {
  while true; do
    # STEP 1: Complete backup
    create_complete_backup
    sleep $SLEEP_INTERVAL # Wait for the defined interval before the next step.
    
    # STEP 2: Incremental backup after STEP 1
    create_incremental_backup 2 1
    sleep $SLEEP_INTERVAL # Wait for the defined interval before the next step.
    
    # STEP 3: Incremental backup after STEP 2
    create_incremental_backup 3 2
    sleep $SLEEP_INTERVAL # Wait for the defined interval before the next step.
    
    # STEP 4: Differential backup after STEP 1
    create_differential_backup
    sleep $SLEEP_INTERVAL # Wait for the defined interval before the next step.
    
    # STEP 5: Incremental backup after STEP 4
    create_incremental_backup 5 4
    sleep $SLEEP_INTERVAL # Wait for the defined interval before the next step.
  done
}

# Execute the main function
main