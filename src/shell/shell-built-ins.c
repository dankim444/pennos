#include "shell-built-ins.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include "../lib/Vec.h"
#include "../kernel/kern_pcb.h" // TODO --> this is a little dangerous, 
                                // make sure not to use k funcs
#include "../kernel/kern_sys_calls.h"
#include "../fs/fs_syscalls.h"
#include <string.h>
#include "../lib/spthread.h"


#include <unistd.h> // probably delete once done 
#include <stdio.h> // I think this is okay? Using snprintf



extern Vec current_pcbs;

extern pcb_t* current_running_pcb; // DELETE


////////////////////////////////////////////////////////////////////////////////
//        The following shell built-in routines should run as                 //
//        independently scheduled PennOS processes.                           //
////////////////////////////////////////////////////////////////////////////////


void* cat(void *arg) {
    // TODO --> implement cat
    return NULL;
}

void* b_sleep(void *arg) {
    // TODO --> implement sleep
    return NULL;
}

void* busy(void *arg) {
    while(1);
    s_exit();
    return NULL;
}

void* echo(void *arg) {
    // TODO --> implement echo
    return NULL;
}

void* ls(void *arg) {
    // TODO --> implement ls
    return NULL;
}

void* touch(void *arg) {
    // TODO --> implement touch
    return NULL;
}

void* mv(void *arg) {
    // TODO --> implement mv
    return NULL;
}

void* cp(void *arg) {
    // TODO --> implement cp
    return NULL;
}

void* rm(void *arg) {
    // TODO --> implement rm
    return NULL;
}

void* chmod(void *arg) {
    // TODO --> implement chmod
    return NULL;
}


/**
 * @brief List all processes on PennOS, displaying PID, PPID, priority, status,
 * and command name.
 *
 * Example Usage: ps
 */
void* ps(void *arg) {
    char pid_top[] = "PID\tPPID\tPRI\tSTAT\tCMD\n";
    write(STDOUT_FILENO, pid_top, strlen(pid_top)); // replace w/ s_write
    for (int i = 0; i < vec_len(&current_pcbs); i++) {
        pcb_t* curr_pcb = (pcb_t*) vec_get(&current_pcbs, i);
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%c\t%s\n", 
                 curr_pcb->pid, curr_pcb->par_pid, curr_pcb->priority, 
                 curr_pcb->process_state, curr_pcb->cmd_str); // TODO --> is this allowed?
        write(STDOUT_FILENO, buffer, strlen(buffer)); // replace w/ s_write
    }

    return NULL;
}

void* kill(void *arg) {
    // TODO --> implement kill
    return NULL;
}



////////////////////////////////////////////////////////////////////////////////
//     The following shell built-ins should be run as the shell; that is,     //         
//     they should each execute as a shell sub-routine directly rather        //
//     than as an independent process.                                        //
////////////////////////////////////////////////////////////////////////////////



void* b_nice(void* arg) {
    // TODO --> implement nice
    return NULL;
}

void* nice_pid(void* arg) {
    // TODO --> implement nice_pid
    return NULL;
}

void* man(void* arg) {
    // TODO --> implement man
    return NULL;
}

void* bg(void* arg) {
    // TODO --> implement bg
    return NULL;
}

void* fg(void* arg) {
    // TODO --> implement fg
    return NULL;
}

void* jobs(void* arg) {
    // TODO --> implement jobs
    return NULL;
}

void* logout(void* arg) {
    // TODO --> implement logout
    return NULL;
}


////////////////////////////////////////////////////////////////////////////////
//     Zombie and orphan testing functions                                    //
////////////////////////////////////////////////////////////////////////////////



/**
 * @brief Helper for zombify.
 */
void* zombie_child(void* arg) {
    s_exit();
    return NULL;
}
  
void* zombify(void* arg) {
    char* zombie_child_argv[] = {"zombie_child", NULL};
    s_spawn(zombie_child, zombie_child_argv, 0, 1); // TODO --> check these fds
    while (1);
    return NULL; 
}
  
/** 
 * @brief Helper for orphanify.
 */
void* orphan_child(void* arg) {
    while (1);
    s_exit();
}

void* orphanify(void* arg) {
    char* orphan_child_argv[] = {"orphan_child", NULL};
    s_spawn(orphan_child, orphan_child_argv, 0, 1); //TODO --> fix/fill in args
    return NULL; // TODO --> this was originally just 'return', update if needed
}