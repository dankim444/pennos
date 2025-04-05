#ifndef KERN_PCB_H_
#define KERN_PCB_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h> 
#include "lib/spthread.h"
#include <lib/Vec.h>



////////////////////////////////////////////////////////////////////////////////
//              PROCESS CONTROL BLOCK (PCB) STRUCTURE AND FUNCTIONS           //
////////////////////////////////////////////////////////////////////////////////

typedef struct pcb_st {
    spthread_t thread_handle; 

    pid_t pid;               
    pid_t par_pid;           // -1 if no parent
    Vec child_pids;         

    int priority;            // priority level (0,1,2)
    char process_state;      // 'R' = running, 'S' = stopped,
                             // 'B' = blocked, 'Z' = zombied

    char* cmd_str;           // str containing command

    bool signals[3];         // signals to be sent to the process (true if
                             // signal still has to be handled)
                             // 0 = P_SIGSTOP, 1 = P_SIGCONT, 2 = P_SIGTERM

    // TODO --> file descriptor table
} pcb_t;

/**
 * @brief Creates a new PCB and initializes its fields
 * 
 * @param thread_handle thread handle for associated spthread
 * @param pid           the new process id
 * @param par_pid       the parent process id
 * @param priority      the priority level (0,1,2)
 * @param cmd_str       the command name as a malloced string ptr
 * 
 * @return pointer to the newly created and malloced PCB
 */
pcb_t* create_pcb(spthread_t thread_handle, pid_t pid, pid_t par_pid, int priority, char* cmd_str);


/**
 * @brief Frees all malloced memory associated with the PCB.
 * 
 * @param pcb Pointer to the PCB to be freed.
 */
void free_pcb(void* pcb);


////////////////////////////////////////////////////////////////////////////////
//        KERNEL-LEVEl PROCESS-RELATED REQUIRED KERNEL FUNCTIONS              //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Create a new child process, inheriting applicable properties from the parent.
 *
 * @return Reference to the child PCB.
 */
pcb_t* k_proc_create(pcb_t *parent);

/**
 * @brief Clean up a terminated/finished thread's resources.
 * This may include freeing the PCB, handling children, etc.
 */
void k_proc_cleanup(pcb_t *proc);


#endif