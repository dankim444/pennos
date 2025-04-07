#include "lib/Vec.h"
#include "kernel/kern_pcb.h"
#include "kernel/scheduler.h"
#include "kernel/kern_sys_calls.h"
#include "fs/fs_syscalls.h"
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <kernel/logger.h>
#include "shell/builtins.h"


extern int tick_counter;
extern int log_fd;

int main(int argc, char* argv[]) {
    
    // get the log fd
    if (argc > 2) {
        log_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    } else {
        log_fd = open("log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }


    // initialize scheduler architecture and init process
    initialize_scheduler_queues();
    
    pid_t init_pid = s_spawn_init();
    if (init_pid == -1) {
        u_perror("init spawn failed"); // TODO --> possible update with new funcs
        exit(EXIT_FAILURE);          // in case exit is illegal or perror is preferred
    }

    scheduler();

    /*

    // cleanup
    free_scheduler_queues();
    close(log_fd);*/
    // TODO --> add anything else that's needed
}