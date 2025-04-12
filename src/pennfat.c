#include "shell/parser.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// function declarations for special routines
static void mkfs(const char *fs_name, int blocks_in_fat, int block_size_config) {
    return;
}
static int mount(const char *fs_name) {
    return 0;
}
static int unmount() {
    return 0;
}

// Signal handler function
static void signal_handler(int signum) {
    // Do nothing for SIGINT and SIGTSTP - just ignore them
}

int main(int argc, char *argv[]) {
    // Register signal handlers
    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    // Set up handler for SIGINT (ctrl-c)
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error setting up SIGINT handler");
        return EXIT_FAILURE;
    }

    // Set up handler for SIGTSTP (ctrl-z)
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error setting up SIGTSTP handler");
        return EXIT_FAILURE;
    }

    //while (1) {
        // TODO: prompt, read command, parse command

        // TODO: execute
        // char **args = parsed_command->commands[0];
        // if (strcmp(args[0], "ls") == 0)
        // {
        //   TODO: Call your implemented ls() function
        // }
        // else if (strcmp(args[0], "touch") == 0)
        // {
        //   TODO: Call your implemented touch() function
        // }
        // else if (strcmp(args[0], "cat") == 0)
        // {
        //   TODO: Call your implemented cat() function
        // }
        // else if (strcmp(args[0], "chmod") == 0)
        // {
        //   TODO: Call your implemented chmod() function
        // }
        // ...
        // 
        // else
        // {
        //   fprintf(stderr, "pennfat: command not found: %s\n", args[0]);
        // }
    //}
    return EXIT_SUCCESS;
}