#include <fcntl.h>
#include <kernel/logger.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include "fs/fs_syscalls.h"
#include "kernel/kern_pcb.h"
#include "kernel/kern_sys_calls.h"
#include "kernel/scheduler.h"
#include "lib/Vec.h"
#include "shell/builtins.h"

#include <stdio.h>  // DELETE

extern int tick_counter;
extern int log_fd;
extern Vec current_pcbs;

int main(int argc, char* argv[]) {
  // get the log fd
  if (argc > 2) {
    log_fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
  } else {
    log_fd = open("log/log", O_RDWR | O_CREAT | O_TRUNC, 0644);
  }

  // initialize scheduler architecture and init process
  initialize_scheduler_queues();

  pid_t init_pid = s_spawn_init();
  if (init_pid == -1) {
    u_perror("init spawn failed");  // TODO --> possible update with new funcs
    exit(EXIT_FAILURE);  // in case exit is illegal or perror is preferred
  }

  scheduler();

  // Init process has PID 1.
  k_proc_cleanup(get_pcb_in_queue(&current_pcbs, 1));
  // cleanup
  free_scheduler_queues();
  close(log_fd);
  // TODO --> add anything else that's needed
}