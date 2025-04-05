#include "kern_sys_calls.h"
#include "lib/Vec.h"
#include "kern-pcb.h"
#include "fs/fs_kfuncs.h"
#include "logger.h"


extern Vec zero_priority_queue; // lower index = more recent 
extern Vec one_priority_queue;
extern Vec two_priority_queue;
extern Vec zombie_queue;
extern Vec sleep_blocked_queue;

extern Vec current_pcbs;


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

/**
 * @brief Set the priority of the specified thread.
 *
 * @param pid Process ID of the target thread.
 * @param priority The new priorty value of the thread (0, 1, or 2)
 * @return 0 on success, -1 on failure.
 */
int s_nice(pid_t pid, int priority) {

    if (priority < 0 || priority > 2) { // error check
        return -1;
    }

    for (int i = 0; i < vec_len(&current_pcbs); i++) {
        pcb_t* curr_pcb = (pcb_t*) vec_get(&current_pcbs, i);
        if (curr_pcb->pid == pid) {
            move_pcb_correct_queue(curr_pcb->priority, priority, curr_pcb);
            log_nice_event(pid, curr_pcb->priority, priority, curr_pcb->cmd_str);
            curr_pcb->priority = priority;
            return 0;
        }
    }

    return -1; // pid not found
}


void s_sleep(unsigned int ticks) {
    // TODO --> implement s_sleep
    return;
}