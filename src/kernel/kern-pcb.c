#include "kern-pcb.h" 
#include "stdlib.h"
#include "stdio.h" // for perror
#include "scheduler.h"
#include "logger.h"

int next_pid = 1; // global variable to track the next pid to be assigned
                  // Note: when incrementing, be careful to lock around
                  // incrementation

extern Vec current_pcbs;


////////////////////////////////////////////////////////////////////////////////
//                            PCB FUNCTIONS                                   //
////////////////////////////////////////////////////////////////////////////////

// TODO --> update this function as pcb changes
void free_pcb(void* pcb) {
    pcb_t* casted_pcb = (pcb_t*) pcb;

    free(casted_pcb->cmd_str);
    vec_destroy(&casted_pcb->child_pids); // observe will free any remaining
                                          // children too!
    
    // TODO --> free file descriptor table

    free(casted_pcb);
}


// TODO --> update this function as pcb changes
pcb_t* create_pcb(spthread_t thread_handle, pid_t pid, pid_t par_pid, int priority, char* cmd_str, int input_fd, int output_fd) {
    pcb_t *ret_pcb = malloc(sizeof(pcb_t));
    if (ret_pcb == NULL) {
        perror("malloc failed for PCB creation");
    }

    ret_pcb->thread_handle = thread_handle;
    ret_pcb->pid = pid;
    ret_pcb->par_pid = par_pid;
    ret_pcb->priority = priority;
    ret_pcb->process_state = 'R'; // running by default
    ret_pcb->cmd_str = cmd_str;
    ret_pcb->input_fd = input_fd;
    ret_pcb->output_fd = output_fd;

    ret_pcb->child_pids = vec_new(0, free_pcb);

    for (int i = 0; i < 3; i++) {
        ret_pcb->signals[i] = false;
    }

    return ret_pcb;
}


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
pcb_t* k_proc_create(pcb_t *parent, int priority) {
    pcb_t* child = malloc(sizeof(pcb_t));
    if (child == NULL) {
        perror("malloc failed in k_proc_create");
        return NULL;
    }

    // set child attributes
    child->pid = next_pid++; // TODO --> check if need locking herre since not atomic
    child->par_pid = parent->pid;
    child->child_pids = vec_new(0, free_pcb);
    child->priority = priority;
    child->process_state = 'R'; // initially running
    for (int i = 0; i < 3; i++) { // "clean slate" of signals
        child->signals[i] = false;
    }
    child->input_fd = parent->input_fd;
    child->output_fd = parent->output_fd;
    // TODO --> copy file descriptor table once added

    // update parent as needed
    vec_push_back(&parent->child_pids, child);

    // add to appropriate queue (TODO -> see if this is necessary)
    put_pcb_into_correct_queue(child);

    return child;
}

void k_proc_cleanup(pcb_t *proc) {

    // if proc has children, remove them and assign new parent
    if (vec_len(&proc->child_pids) > 0 ) {
        pcb_t* par_pcb = get_pcb_in_queue(&current_pcbs, proc->par_pid);
        if (par_pcb != NULL) {
            while (vec_len(&proc->child_pids) > 0) {
                pcb_t* curr_child = vec_get(&proc->child_pids, 0);
                vec_push_back(&par_pcb->child_pids, curr_child); // add to new parent
                vec_erase_no_deletor(&proc->child_pids, 0); // don't free in erase
                curr_child->par_pid = proc->par_pid; // update parent
                log_generic_event('O', curr_child->pid, curr_child->priority, curr_child->cmd_str);
            }
        }        
    }

    // TODO --> handle fd table once added

    // delete this process from any queue it's in + free it
    delete_process_from_all_queues(proc);
    free_pcb(proc);
}
