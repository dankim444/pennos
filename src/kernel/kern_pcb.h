#ifndef KERN_PCB_H_
#define KERN_PCB_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h> 
#include "../lib/spthread.h"
#include "../lib/Vec.h"

#define MAX_FDS 16  // TODO: Determine the maximum number of file descriptors per process

////////////////////////////////////////////////////////////////////////////////
//              PROCESS CONTROL BLOCK (PCB) STRUCTURE AND FUNCTIONS           //
////////////////////////////////////////////////////////////////////////////////

typedef struct fd_entry {
    bool in_use;          // Whether this file descriptor is in use
    int flags;            // Open mode flags (F_READ, F_WRITE, F_APPEND)
    int offset;           // Current file position
    char* filename;       // Pointer to filename (dynamically allocated)
    uint16_t start_block; // First block in FAT
} fd_entry_t;

typedef struct pcb_st {
    spthread_t thread_handle; 

    pid_t pid;               
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
                             // 0 otherwise


    fd_entry_t fd_table[MAX_FDS];  // File descriptor table
} pcb_t;

/**
 * @brief Creates a new PCB and initializes its fields
 * 
 * @param thread_handle thread handle for associated spthread
 * @param pid           the new process id
 * @param par_pid       the parent process id
 * @param priority      the priority level (0,1,2)
 * @param cmd_str       the command name as a malloced string ptr
 * @param input_fd      input fd
 * @param output_fd     output fd
 * 
 * @return pointer to the newly created and malloced PCB
 */
pcb_t* create_pcb(spthread_t thread_handle, pid_t pid, pid_t par_pid, int priority, char* cmd_str, int input_fd, int output_fd);


/**
 * @brief Frees all malloced memory associated with the PCB. Note that 
 *        this will destroy children too, so be careful when using it.
 *        In particular, make sure to remove any children pcbs you want
 *        to preserve.
 * 
 * @param pcb Pointer to the PCB to be freed, NULL if error
 */
void free_pcb(void* pcb);


////////////////////////////////////////////////////////////////////////////////
//        KERNEL-LEVEl PROCESS-RELATED REQUIRED KERNEL FUNCTIONS              //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Create a new child process, inheriting applicable properties from the parent.
 *
 * @param parent a pointer to the parent pcb
 * @param priority the priority of the child, usually 1 but exceptions like shell exist
 * @return Reference to the child PCB.
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