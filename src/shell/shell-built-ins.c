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


#include <unistd.h> // probably delete once done 
#include <stdio.h> // I think this is okay? Using snprintf



extern Vec current_pcbs;


////////////////////////////////////////////////////////////////////////////////
//        The following shell built-in routines should run as                 //
//        independently scheduled PennOS processes.                           //
////////////////////////////////////////////////////////////////////////////////


void* b_cat(void *arg) {
    // TODO --> implement cat
    return NULL;
}

void* b_sleep(void *arg) {
    // TODO --> implement sleep
    return NULL;
}

void* b_busy(void *arg) {
    // TODO --> implement busy
    return NULL;
}

void* b_echo(void *arg) {
    // TODO --> implement echo
    return NULL;
}

void* b_ls(void *arg) {
    // TODO --> implement ls
    return NULL;
}

void* b_touch(void *arg) {
    // TODO --> implement touch
    return NULL;
}

void* b_mv(void *arg) {
    // TODO --> implement mv
    return NULL;
}

void* b_cp(void *arg) {
    // TODO --> implement cp
    return NULL;
}

void* b_rm(void *arg) {
    // TODO --> implement rm
    return NULL;
}

void* b_chmod(void *arg) {
    // TODO --> implement chmod
    return NULL;
}


/**
 * @brief List all processes on PennOS, displaying PID, PPID, priority, status,
 * and command name.
 *
 * Example Usage: ps
 */
void* b_ps(void *arg) {
    write(STDOUT_FILENO, "PID\tPPID\tPRI\tSTAT\tCMD\n", 35); // replace w/ s_write
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

void* b_kill(void *arg) {
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

void* b_nice_pid(void* arg) {
    // TODO --> implement nice_pid
    return NULL;
}

void* b_man(void* arg) {
    // TODO --> implement man
    return NULL;
}

void* b_bg(void* arg) {
    // TODO --> implement bg
    return NULL;
}

void* b_fg(void* arg) {
    // TODO --> implement fg
    return NULL;
}

void* b_jobs(void* arg) {
    // TODO --> implement jobs
    return NULL;
}

void* b_logout(void* arg) {
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
    return NULL; // TODO --> this was originally just 'return', update if needed
}
  
void* zombify(void* arg) {
    //s_spawn(zombie_child, ...); TODO --> fix/fill in args
    while (1);
    return NULL; // TODO --> this was originally just 'return', update if needed
}
  
/** 
 * @brief Helper for orphanify.
 */
void* orphan_child(void* arg) {
    while (1);
}

void* orphanify(void* arg) {
    // s_spawn(orphan_child, ...); TODO --> fix/fill in args
    return NULL; // TODO --> this was originally just 'return', update if needed
}