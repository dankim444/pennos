/* CS5480 PennOS Group 61
 * Authors: Krystof Purtell and Richard Zhang
 * Purpose: Defines the PCB stucture, process-related helper functions,
 *          and the kernel-level process functions.  
 */

#ifndef KERN_PCB_H_
#define KERN_PCB_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h> 
#include "../lib/spthread.h"
#include "../lib/Vec.h"


#define FILE_DESCRIPTOR_TABLE_SIZE 100

////////////////////////////////////////////////////////////////////////////////
//              PROCESS CONTROL BLOCK (PCB) STRUCTURE AND FUNCTIONS           //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief The PCB structure, which contains all the information about a process.
 *        Notably, it contains the thread handle, pid, parent pid, child pcbs,
 *        priority level, process state, command string, signals to be sent,
 *        input and output file descriptors, process status, sleeping status,
 *        and time to wake.
 */
typedef struct pcb_st {
    spthread_t thread_handle; 

    pid_t pid;               // 0 if init 
    pid_t par_pid;           // -1 if no parent

    Vec child_pcbs;          // pcb ptrs to children, not ints        

    int priority;            // priority level (0,1,2)
    char process_state;      // 'R' = running, 'S' = stopped,
                             // 'B' = blocked, 'Z' = zombied

    char* cmd_str;           // str containing command

    bool signals[3];         // signals to be sent to the process (true if
                             // signal still has to be handled)
                             // 0 = P_SIGSTOP, 1 = P_SIGCONT, 2 = P_SIGTERM

    int input_fd;
    int output_fd;

    int process_status;      // process status 
                             // EXITED_NORMALLY 20
                             // STOPPED_BY_SIG 21
                             // TERM_BY_SIG 22
                             // CONT_BY_SIG 23
                             // 0 otherwise

    bool is_sleeping;
    int time_to_wake; // time to wake up if sleeping, -1 if not sleeping

    int fd_table[FILE_DESCRIPTOR_TABLE_SIZE]; // file descriptor table (-1 if not in use)
} pcb_t;

////////////////////////////////////////////////////////////////////////////////
//                               PCB FUNCTIONS                                //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Creates a new PCB and initializes its fields. Notably, the thread handle
 *        and cmd are left out. It's up to the user to assign them post-call.
 * 
 * @param pid           the new process id
 * @param par_pid       the parent process id
 * @param priority      the priority level (0,1,2)
 * @param input_fd      input fd
 * @param output_fd     output fd
 * 
 * @return pointer to the newly created and malloced PCB or NULL if failure
 */
pcb_t* create_pcb(pid_t pid, pid_t par_pid, int priority, int input_fd, int output_fd);


/**
 * @brief Frees all malloced memory associated with the PCB. Note that 
 *        this will destroy children too, so be careful when using it.
 *        In particular, make sure to remove any children pcbs you want
 *        to preserve.
 * 
 * @param pcb Pointer to the PCB to be freed, NULL if error
 */
void free_pcb(void* pcb);

/**
 * @brief Given a parent, removes the child from the parent's child
 *        vector if its exists. Notably, it does not free the child but
 *        simply removes it via the vec_erase_no_deletor function.
 * 
 * @param parent a ptr to the parent pcb with the child list 
 * @param child  a ptr to the child pcb that we'd like to remove
 */
void remove_child_in_parent(pcb_t* parent, pcb_t* child);

////////////////////////////////////////////////////////////////////////////////
//        KERNEL-LEVEl PROCESS-RELATED REQUIRED KERNEL FUNCTIONS              //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Create a new child process, inheriting applicable properties from the parent.
 *        Also inserts the created child into the correct scheduler queue based on its
 *        priority.
 * @param parent a pointer to the parent pcb
 * @param priority the priority of the child, usually 1 but exceptions like shell exist
 * @return Reference to the child PCB or NULL if error
 */
pcb_t* k_proc_create(pcb_t *parent, int priority);

/**
 * @brief Clean up a terminated/finished thread's resources. This may include
 *        freeing the PCB, handling children, etc. If a child is orphaned, the
 *        INIT process becomes its parent.
 * 
 * @param proc a pcb ptr to the terminated/finished thread
 */
void k_proc_cleanup(pcb_t *proc);

#endif
