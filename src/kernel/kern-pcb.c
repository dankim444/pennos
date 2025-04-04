#include "kern-pcb.h" 
#include "stdlib.h"



////////////////////////////////////////////////////////////////////////////////
//                            PCB FUNCTIONS                                   //
////////////////////////////////////////////////////////////////////////////////

// TODO --> update this function as pcb changes
void free_pcb(pcb_t *pcb) {
    free(pcb->cmd);
    vec_destroy(&pcb->child_pids);
    // TODO --> free file descriptor table

    free(pcb);
}


// TODO --> update this function as pcb changes
pcb_t* create_pcb(spthread_t thread_handle, pid_t pid, pid_t par_pid, int priority, char* cmd) {
    pcb_t *ret_pcb = malloc(sizeof(pcb_t));
    if (ret_pcb == NULL) {
        perror("malloc failed for PCB creation");
    }

    ret_pcb->thread_handle = thread_handle;
    ret_pcb->pid = pid;
    ret_pcb->par_pid = par_pid;
    ret_pcb->priority = priority;
    ret_pcb->process_state = 'R'; // running by default
    ret_pcb->cmd = cmd;

    ret_pcb->child_pids = vec_new(0, free);

    for (int i = 0; i < 3; i++) {
        ret_pcb->signals[i] = false;
    }

    return ret_pcb;
}


////////////////////////////////////////////////////////////////////////////////
//        KERNEL-LEVEl PROCESS-RELATED REQUIRED KERNEL FUNCTIONS              //
////////////////////////////////////////////////////////////////////////////////


pcb_t* k_proc_create(pcb_t *parent) {
    // TODO: Implement the process creation logic
    return NULL;
}

void k_proc_cleanup(pcb_t *proc) {
    // TODO: Implement the process cleanup logic
    return;
}
