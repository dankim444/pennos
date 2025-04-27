#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "fs/fs_syscalls.h"
#include "kernel/kern_sys_calls.h"
#include "kernel/scheduler.h"
#include "shell/builtins.h"
#include "lib/pennos-errno.h"
#include "fs/fat_routines.h"

extern int tick_counter;
extern int log_fd;

int main(int argc, char* argv[]) {
  // mount the filesystem
  if (argc < 2) {
    P_ERRNO = P_NEEDF;
    u_perror("need a pennfat file to mount");
    return -1;
  } else {
    if (mount(argv[1]) == -1) {
      u_perror("mount failed");
      return -1;
    }
  }

  // get the log fd
  if (argc >= 3) {
    log_fd = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
  } else {
    log_fd = open("log/log", O_RDWR | O_CREAT | O_TRUNC, 0644);
  }

  // initialize scheduler architecture and init process
  initialize_scheduler_queues();

  pid_t init_pid = s_spawn_init();
  if (init_pid == -1) {
    P_ERRNO = P_INITFAIL;
    u_perror("init spawn failed"); 
    return -1;
  }

  scheduler();

  // cleanup
  s_cleanup_init_process();
  free_scheduler_queues();
  unmount();
  close(log_fd);
}