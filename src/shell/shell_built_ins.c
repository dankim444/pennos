#include "shell_built_ins.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>


////////////////////////////////////////////////////////////////////////////////
//        The following shell built-in routines should run as                 //
//        independently scheduled PennOS processes.                           //
////////////////////////////////////////////////////////////////////////////////


void* cat(void *arg) {
    // TODO --> implement cat
    return NULL;
}

void* sleep(void *arg) {
    // TODO --> implement sleep
    return NULL;
}

void* busy(void *arg) {
    // TODO --> implement busy
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

void* ps(void *arg) {
    // TODO --> implement ps
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



void* nice(void* arg) {
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