#include "kern_sys_calls.h"
#include "../lib/Vec.h"
#include "kern_pcb.h"
#include "../fs/fs_kfuncs.h"
#include "logger.h"
#include "signal.h"
#include "../shell/builtins.h"
#include "../lib/pennos-errno.h"
#include <string.h>
#include "scheduler.h"
#include "../shell/shell.h" 
#include "logger.h"

#include "stdio.h" // TODO: delete this once finished
 

extern Vec zero_priority_queue; // lower index = more recent 
extern Vec one_priority_queue;
extern Vec two_priority_queue;
extern Vec zombie_queue;
extern Vec sleep_blocked_queue;
extern Vec current_pcbs;

extern pcb_t* current_running_pcb; // currently running process


////////////////////////////////////////////////////////////////////////////////
//                         GENERAL HELPER FUNCTIONS                           //
////////////////////////////////////////////////////////////////////////////////



/** 
 * @brief Given a thread pid and Vec* queue, this helper function determines 
 *        the vector index of the thread/pid in the queue. If the thread/pid 
 *        is not found, it returns -1.
 * 
 * @param queue pointer to the vector queue that may contain the thread/pid
 * @param pid   the thread's pid
 * 
 * @return the index of the thread/pid in the queue, or -1 if not found
 */
int determine_index_in_queue(Vec *queue, int pid) {
    for (int i = 0; i < vec_len(queue); i++) {
        pcb_t* curr_pcb = vec_get(queue, i);
        if (curr_pcb->pid == pid) {
            return i;
        }
    }

    return -1; // not found
}

/**
 * @brief Given a thread's previous priority, this helper checks if the 
 *        thread is present in that priority's queue, removes it from that 
 *        queue if so, and then puts it into the new priority level's queue.
 * 
 * @param prev_priority thread's previous priority
 * @param new_priority  thread's new priority
 * @param curr_pcb     pointer to the thread's PCB
 * 
 * @pre assumes the prev_priority and new_priority falls in integers [0, 2]
 */
void move_pcb_correct_queue(int prev_priority, int new_priority, pcb_t* curr_pcb) {
    Vec* prev_queue;
    Vec* new_queue;

    if (prev_priority == 0) {
        prev_queue = &zero_priority_queue;
    } else if (prev_priority == 1) {
        prev_queue = &one_priority_queue;
    } else {
        prev_queue = &two_priority_queue;
    }

    if (new_priority == 0) {
        new_queue = &zero_priority_queue;
    } else if (new_priority == 1) {
        new_queue = &one_priority_queue;
    } else {
        new_queue = &two_priority_queue;
    }

    // delete from prev_queue, if it's present at all
    int ind = determine_index_in_queue(prev_queue, curr_pcb->pid);
    if (ind != -1) {
        vec_erase_no_deletor(prev_queue, ind);
    }

    vec_push_back(new_queue, curr_pcb);
}



////////////////////////////////////////////////////////////////////////////////
//        SYSTEM-LEVEl PROCESS-RELATED KERNEL FUNCTIONS                       //
////////////////////////////////////////////////////////////////////////////////

void* init_func(void* input) {

    // spawns in the shell
    char* shell_argv[] = {"shell_main", NULL};
    s_spawn(shell_main, shell_argv, 0, 1); // TODO: check these fds

    // TODO --> determine if we should wait here

    return NULL; // needed for void* output
}

pid_t s_spawn_init() {
    pcb_t* init = k_proc_create(NULL, 0);
    if (init == NULL) {
        P_ERRNO = P_NULL;
        return -1;
    }

    spthread_t thread_handle;
    if (spthread_create(&thread_handle, NULL, init_func, NULL) != 0) {
        P_ERRNO = P_EINTR; 
        return -1;
    }

    init->cmd_str = strdup("init");
    init->thread_handle = thread_handle;
    return init->pid; 
}

pid_t s_spawn(void* (*func)(void*), char *argv[], int fd0, int fd1) {
    pcb_t* child;
    if (strcmp(argv[0], "shell_main") == 0) { // shell, unlike others, has priority 1
        child = k_proc_create(current_running_pcb, 0);
    } else {
        child = k_proc_create(current_running_pcb, 1);
    }
   
    if (child == NULL) {
        P_ERRNO = P_NULL;
        return -1;
    }

    spthread_t thread_handle; 

    if (spthread_create(&thread_handle, NULL, func, argv) != 0) {
        P_ERRNO = P_EINTR; // im removing u_perror here bc i believe the shell is only allowed to call u_perror 
                            // the kernel + fs just sets the type of error and returns -1, then the shell catches the error 
                            // and interprets it using u_perror. -Dan
        return -1;
    }

    child->cmd_str = strdup(argv[0]);
    child->thread_handle = thread_handle;
    child->input_fd = fd0;
    child->output_fd = fd1;

    log_generic_event('C', child->pid, child->priority, child->cmd_str);

    return child->pid; // return child->pid if successful
}

pid_t s_waitpid(pid_t pid, int* wstatus, bool nohang) {
    // TODO --> implement s_waitpid
    return -1;
}

// TODO --> make sure signals are handled at some point in the loop
int s_kill(pid_t pid, int signal) {
    pcb_t* pcb_with_pid = get_pcb_in_queue(&current_pcbs, pid);
    if (pcb_with_pid == NULL) {
        return -1; // pid not found case
    }

    pcb_with_pid->signals[signal] = true; // signal flagged
    log_generic_event('S', pid, pcb_with_pid->priority, pcb_with_pid->cmd_str);
    return 0;
}

void s_exit(void) {
    current_running_pcb->process_state = 'Z'; // set to zombie
    current_running_pcb->process_status = 20; // exited normally
    log_generic_event('Z', current_running_pcb->pid, current_running_pcb->priority, current_running_pcb->cmd_str);
}

int s_nice(pid_t pid, int priority) {

    if (priority < 0 || priority > 2) { // error check
        return -1;
    }

    pcb_t* curr_pcb = get_pcb_in_queue(&current_pcbs, pid);
    if (curr_pcb != NULL) { // found + exists
        move_pcb_correct_queue(curr_pcb->priority, priority, curr_pcb);
        log_nice_event(pid, curr_pcb->priority, priority, curr_pcb->cmd_str);
        curr_pcb->priority = priority;
        return 0;
    }

    return -1; // pid not found
}


void s_sleep(unsigned int ticks) {
    // TODO --> implement s_sleep
    return;
}