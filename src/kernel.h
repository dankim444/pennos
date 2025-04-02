#ifndef KERNEL_H_
#define KERNEL_H_

#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h> 

typedef struct pcb_st {
    // TODO: Define the PCB structure
} pcb_t;

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