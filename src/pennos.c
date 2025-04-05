#include "src/lib/Vec.h"
#include "src/kernel/kern-pcb.h"
#include "src/kernel/scheduler.h"
#include <stdbool.h>
#include <stdlib.h>
#include <fcntl.h>


int tick_counter = 0;
int log_fd;

int main(char* argv[], int argc) {
    
    // get the log fd
    if (argc > 2) {
        log_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    } else {
        log_fd = open("log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }


    // initialize scheduler architecture and init process
    initialize_scheduler_queues();
    // TODO --> initialzie init process

    scheduler();

    // cleanup
    free_scheduler_queues();
    close(log_fd);
    // TODO --> add anything else that's needed
}