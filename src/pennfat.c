#include "shell/parser.h"
#include "shell/builtins.h" // for u_perror
#include "lib/pennos-errno.h" // for setting error codes
#include "fs/fs_kfuncs.h"
#include "fs/fat_routines.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

#ifndef PROMPT
#define PROMPT "pennfat# "
#endif

// signal handler function
static void signal_handler(int signum) {
    // do nothing for SIGINT and SIGTSTP - just ignore them
}

int main(int argc, char *argv[]) {
    // register signal handlers
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    // set up handler for SIGINT (ctrl-c)
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        P_ERRNO = P_ESIGNAL;
        u_perror("Error setting up SIGINT handler");
        return EXIT_FAILURE;
    }

    // set up handler for SIGTSTP (ctrl-z)
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        P_ERRNO = P_ESIGNAL;
        u_perror("Error setting up SIGTSTP handler");
        return EXIT_FAILURE;
    }

    char input_buffer[1024];
    
    while (true) {
        // print prompt
        if (k_write(STDOUT_FILENO, PROMPT, strlen(PROMPT)) < 0) {
            P_ERRNO = P_EWRITE;
            u_perror("prompt write error");
            break;
        }
        
        // read user input
        int bytes_read = k_read(STDIN_FILENO, input_buffer, sizeof(input_buffer) - 1);
        
        // check for EOF (ctrl-D)
        if (bytes_read <= 0) {
            k_write(STDOUT_FILENO, "\n", 1);
            break;
        }
        
        // remove trailing newline if present
        if (bytes_read > 0 && input_buffer[bytes_read - 1] == '\n') {
            input_buffer[bytes_read - 1] = '\0';
        }
        
        // parse command and check error
        struct parsed_command *parsed_command = NULL;
        int parse_result = parse_command(input_buffer, &parsed_command);
        if (parse_result != 0) {
            if (parse_result == -1) {
                P_ERRNO = P_EINVAL;
                u_perror("Error parsing command");
            } else {
                print_parser_errcode(stderr, parse_result);
            }
            continue;
        }
        
        // skip empty commands
        if (parsed_command->num_commands == 0) {
            free(parsed_command);
            continue;
        }
        
        // extract command and arguments
        char **args = parsed_command->commands[0];
        
        // execute command
        if (strcmp(args[0], "mkfs") == 0) {
            if (args[1] == NULL || args[2] == NULL || args[3] == NULL) {
                P_ERRNO = P_EINVAL;
                u_perror("mkfs");
            } else {
                int blocks_in_fat = atoi(args[2]);
                int block_size = atoi(args[3]);
                if (mkfs(args[1], blocks_in_fat, block_size) != 0) {
                    u_perror("mkfs");
                }
            }
        } else if (strcmp(args[0], "mount") == 0) {
            if (args[1] == NULL) {
                P_ERRNO = P_EINVAL;
                u_perror("mount");
            } else {
                if (mount(args[1]) != 0) {
                    u_perror("mount");
                }
            }
        } else if (strcmp(args[0], "unmount") == 0) {
            if (unmount() != 0) {
                u_perror("unmount");
            }
        } else if (strcmp(args[0], "ls") == 0) {
            ls(args);
        } else if (strcmp(args[0], "touch") == 0) {
            touch(args);
        } else if (strcmp(args[0], "cat") == 0) {
            cat(args);
        } else if (strcmp(args[0], "chmod") == 0) {
            chmod(args);
        } 
        else if (strcmp(args[0], "mv") == 0) {
            mv(args);
        } else if (strcmp(args[0], "rm") == 0) {
            rm(args);
        } else if (strcmp(args[0], "cp") == 0) {
            cp(args);
        } else if (strcmp(args[0], "chmod") == 0) {
            chmod(args);
        } else {
            P_ERRNO = P_ECOMMAND;
            u_perror("shell");
        }

        // free resource and re-prompt
        free(parsed_command);
    }
    // note any other resources we might need to free (?)
    return EXIT_SUCCESS;
}
