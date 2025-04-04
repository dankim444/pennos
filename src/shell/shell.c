#include "kernel/kern_sys_calls.h"
#include "parser.h"
#include "shell-built-ins.h"
#include "builtins.h" // contains u_perrror
#include <string.h>
#include "fs/fs_syscalls.h"
#include "stdlib.h"

#ifndef PROMPT
#define PROMPT "$"
#endif

#define MAX_BUFFER_SIZE 4096


/**
 * @brief Helper function to execute a parsed command from the shell. 
 * In particular, it spawns a child process to execute the command if 
 * the built-in should run as a separate process. Otherwise, it just
 * calls the subroutine directly.
 * 
 * @param cmd the parsed command to execute, assumed non-null
 * @return the created child id on successful spawn, 0 on successful
 *         subroutine call, -1 on error 
 */
pid_t execute_command(struct parsed_command* cmd) {

    int input_fd = s_open(cmd->stdin_file, F_READ); // TODO --> error check these once implemented 
    int output_fd;
    if (cmd->is_file_append) {
        output_fd = s_open(cmd->stdout_file, F_APPEND);
    } else {
        output_fd = s_open(cmd->stdout_file, F_WRITE);
    }

    // check for independently scheduled processes
    if (strcmp(cmd->commands[0][0], "cat") == 0) {
        // TODO --> make sure these files from parser are okay!
        // I'm assuming yes b/c we implement s_spawn but just check
        return s_spawn(cat, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "sleep") == 0) {
        return s_spawn(sleep, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "busy") == 0) {
        return s_spawn(busy, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "echo") == 0) {
        return s_spawn(echo, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "ls") == 0) {
        return s_spawn(ls, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "touch") == 0) {
        return s_spawn(touch, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "mv") == 0) {
        return s_spawn(mv, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "cp") == 0) {
        return s_spawn(cp, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "rm") == 0) {
        return s_spawn(rm, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "chmod") == 0) {
        return s_spawn(chmod, cmd->commands[0], input_fd, output_fd);
    } else if (strcmp(cmd->commands[0][0], "kill") == 0) {
        return s_spawn(kill, cmd->commands[0], input_fd, output_fd);
    } 

    // check for sub-routines (nice, nice_pid, man, bg, fg, jobs, logout)
    if (strcmp(cmd->commands[0][0], "nice") == 0) {
        nice(cmd->commands[0]);
    } else if (strcmp(cmd->commands[0][0], "nice_pid") == 0) {
        nice_pid(cmd->commands[0]);
    } else if (strcmp(cmd->commands[0][0], "man") == 0) {
        man(cmd->commands[0]);
    } else if (strcmp(cmd->commands[0][0], "bg") == 0) {
        bg(cmd->commands[0]);
    } else if (strcmp(cmd->commands[0][0], "fg") == 0) {
        fg(cmd->commands[0]);
    } else if (strcmp(cmd->commands[0][0], "jobs") == 0) {
        jobs(cmd->commands[0]);
    } else if (strcmp(cmd->commands[0][0], "logout") == 0) {
        logout(cmd->commands[0]);
    } else {
        // TODO --> handle error via some valid print
        return -1;
    }
    
    return 0; // only reached for subroutines
}

/**
 * @brief TODO --> once we figure out what this is supposed to be 
 * 
 * @return 0 on success, -1 on error
 */
int shell_main() {

    // TODO --> determine if we need more here
    // add in the jobs, bg, fg stuff as needed
    // cross-check against penn-shell.c file once we 
    // get answers on this

    while(true) {

        // TODO --> attempt waiting on all children

        // prompt 
        if (s_write(STDOUT_FILENO, PROMPT, strlen(PROMPT)) < 0) {
            u_perror("prompt write error");
            break;
        }

        // parse user input
        char buffer[MAX_BUFFER_SIZE];
        ssize_t user_input = s_read(STDIN_FILENO, MAX_BUFFER_SIZE, buffer);
        if (user_input < 0) {
            u_perror("shell read error");
            break;
        } else if (user_input == 0) { // EOF case
            break;
        }

        buffer[user_input] = '\0';
        if (buffer[user_input - 1] == '\n') {
            buffer[user_input - 1] = '\0';
        }

        struct parsed_command* cmd = NULL;
        int cmd_parse_res = parse_command(buffer, &cmd);
        if (cmd_parse_res != 0 || cmd == NULL) {
            // TODO -> handle error via some valid print
            continue;
        }

        // handle the command
        if (cmd->num_commands == 0) {
            free(cmd);
            continue;
        }

        // TODO --> handle builtins/bg as needed (if needed)


        pid_t child_pid = execute_command(cmd);
        if (child_pid < 0) {
            // TODO --> handle error via some valid print
            free(cmd);
            continue;
        } else if (child_pid > 0) {
            // TODO --> handle child process,
            // see if we need to wait for child to finish
        } 

        // TODO --> free cmd?
    }

    return 0;
}


