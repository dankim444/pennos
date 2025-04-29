#include "logger.h"
#include <stdio.h>
#include <unistd.h>

/**
 * @brief Logs when an event is scheduled
 */
void log_scheduling_event(int pid, int queue_num, char* process_name) {
    char buffer[200];
    int str_len = snprintf(buffer, sizeof(buffer), "[%d]\tSCHEDULE\t%d\t%d\t%s\n", tick_counter, pid, queue_num, process_name);
    if (write(log_fd, buffer, str_len) == -1) {
        perror("error in writing to the log file for scheduling event");
    }
}

/**
 * @brief Logs non-nice, non-scheduling events since they have same format
 */
void log_generic_event(char event_type, int pid, int nice_value, char* process_name) {
    char* operation;

    switch(event_type) {
        case 'C':
            operation = "CREATE";
            break;
        case 'S':
            operation = "SIGNALED";
            break;
        case 'E':
            operation = "EXITED";
            break;
        case 'Z':
            operation = "ZOMBIE";
            break;
        case 'O':
            operation = "ORPHAN";
            break;
        case 'W':
            operation = "WAITED";
            break;
        case 'B':
            operation = "BLOCKED";
            break;
        case 'U':
            operation = "UNBLOCKED";
            break;
        case 's':
            operation = "STOPPED";
            break;
        default:
            operation = "CONTINUED";
            break;
    }

    char buffer[200];
    int str_len = snprintf(buffer, sizeof(buffer), "[%d]\t%s\t%d\t%d\t%s\n", tick_counter, operation, pid, nice_value, process_name);
    if (write(log_fd, buffer, str_len) == -1) {
        perror("error in writing to the log file for generic event");
    }
}

/**
 * @brief Logs a nice-related event
 */
void log_nice_event(int pid, int old_nice_value, int new_nice_value, char* process_name) {
    char buffer[200];
    int str_len = snprintf(buffer, sizeof(buffer), "[%d]\tNICE\t%d\t%d\t%d\t%s\n", tick_counter, pid, old_nice_value, new_nice_value, process_name);
    if (write(log_fd, buffer, str_len) == -1) {
        perror("error in writing to the log file for nice event");
    }
}

