#include "kern_sys_calls.h"

#include "kern-pcb.h"
#include "fs/fs_kfuncs.h"


////////////////////////////////////////////////////////////////////////////////
//        SYSTEM-LEVEl PROCESS-RELATED REQUIRED KERNEL FUNCTIONS              //
////////////////////////////////////////////////////////////////////////////////



pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1) {
    // TODO --> implement s_spawn
    return -1;
}

pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang) {
    // TODO --> implement s_waitpid
    return -1;
}

int s_kill(pid_t pid, int signal) {
    // TODO --> implement s_kill
    return -1;
}

void s_exit(void) {
    // TODO --> implement s_exit
    return;
}

int s_nice(pid_t pid, int priority) {
    // TODO --> implement s_nice
    return -1;
}


void s_sleep(unsigned int ticks) {
    // TODO --> implement s_sleep
    return;
}