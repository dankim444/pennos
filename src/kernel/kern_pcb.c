#include "kern_pcb.h"
#include "../lib/pennos-errno.h"
#include "logger.h"
#include "scheduler.h"
#include "stdio.h"  // for perror
#include "stdlib.h"
#include "../fs/fs_syscalls.h"


int next_pid = 2;  // global variable to track the next pid to be assigned
                   // Note: when incrementing, be careful to lock around
                   // incrementation. Starts at 2 b/c init is 1

extern Vec current_pcbs;
extern pcb_t* current_running_pcb;

////////////////////////////////////////////////////////////////////////////////
//                            PCB FUNCTIONS                                   //
////////////////////////////////////////////////////////////////////////////////

void free_pcb(void* pcb) {
  pcb_t* casted_pcb = (pcb_t*)pcb;

  free(casted_pcb->cmd_str);
  vec_destroy(&casted_pcb->child_pcbs);  // observe will free any remaining
                                         // children too!
  free(casted_pcb);
}

pcb_t* create_pcb(pid_t pid,
                  pid_t par_pid,
                  int priority,
                  int input_fd,
                  int output_fd) {
  pcb_t* ret_pcb = malloc(sizeof(pcb_t));
  if (ret_pcb == NULL) {
    perror("malloc failed for PCB creation");
    return NULL;
  }

  ret_pcb->pid = pid;
  ret_pcb->par_pid = par_pid;
  ret_pcb->priority = priority;
  ret_pcb->process_state = 'R';  // running by default
  ret_pcb->input_fd = input_fd;
  ret_pcb->output_fd = output_fd;
  ret_pcb->process_status = 0;  // default status

  // changed the deconstructor to NULL because when exiting don't want PennOS to
  // double free
  ret_pcb->child_pcbs = vec_new(0, NULL);

  for (int i = 0; i < 3; i++) {
    ret_pcb->signals[i] = false;
  }

  ret_pcb->is_sleeping = false;
  ret_pcb->time_to_wake = -1;  // default to not sleeping

  return ret_pcb;
}

void remove_child_in_parent(pcb_t* parent, pcb_t* child) {
  for (int i = 0; i < vec_len(&parent->child_pcbs); i++) {
    pcb_t* curr_child = (pcb_t*)vec_get(&parent->child_pcbs, i);
    if (curr_child->pid == child->pid) {
      vec_erase_no_deletor(&parent->child_pcbs, i);
      return;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
//        KERNEL-LEVEl PROCESS-RELATED REQUIRED KERNEL FUNCTIONS              //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Create a new child process, inheriting applicable properties from the
 * parent.
 *
 * @param parent a pointer to the parent pcb
 * @param priority the priority of the child, usually 1 but exceptions like
 * shell exist
 * @return Reference to the child PCB.
 */
pcb_t* k_proc_create(pcb_t* parent, int priority) {
  if (parent == NULL) {                       // init creation case
    pcb_t* init = create_pcb(1, 0, 0, 0, 1);  // TODO: check these fds
    if (init == NULL) {
      P_ERRNO = P_ENULL;  // TODO --> do we want this?
    }
    init->fd_table[0] = STDIN_FILENO;
    init->fd_table[1] = STDOUT_FILENO;
    init->fd_table[2] = STDERR_FILENO;
    for (int i = 3; i < FILE_DESCRIPTOR_TABLE_SIZE; i++) {
      init->fd_table[i] = -1;
    }

    current_running_pcb = init;
    put_pcb_into_correct_queue(init);
    vec_push_back(&current_pcbs, init);
    return init;
  }

  pcb_t* child = create_pcb(next_pid++, parent->pid, priority, parent->input_fd,
                            parent->output_fd);
  if (child == NULL) {
    P_ERRNO = P_ENULL;  // TODO --> do we want this?
    return NULL;
  }

  // copy parent's fd table
  for (int i = 0; i < FILE_DESCRIPTOR_TABLE_SIZE; i++) { 
    child->fd_table[i] = parent->fd_table[i];
  }
  // TODO --> file system heads should increase reference count

  // update parent as needed
  vec_push_back(&parent->child_pcbs, child);

  // add to appropriate queue
  put_pcb_into_correct_queue(child); 
  vec_push_back(&current_pcbs, child);

  return child;
}

void k_proc_cleanup(pcb_t* proc) {
  // if proc has parent (i.e. isn't init) then remove it from parent's child
  // list
  pcb_t* par_pcb = get_pcb_in_queue(&current_pcbs, proc->par_pid);
  if (par_pcb != NULL) {
    remove_child_in_parent(par_pcb, proc);
  } else {
    P_ERRNO = P_ENULL;  // TODO --> do we want this?
    return;
  }

  // if proc has children, remove them and assign them to init parent
  if (vec_len(&proc->child_pcbs) > 0) {
    // retrieve the init process
    pcb_t* init_pcb =
        get_pcb_in_queue(&current_pcbs, 1);  // init process has pid 1

    while (vec_len(&proc->child_pcbs) > 0) {
      pcb_t* curr_child = vec_get(&proc->child_pcbs, 0);
      vec_push_back(&init_pcb->child_pcbs, curr_child);
      vec_erase_no_deletor(&proc->child_pcbs, 0);  // don't free in erase
      curr_child->par_pid = 1;  // update parent to init (pid 1)
      log_generic_event('O', curr_child->pid, curr_child->priority,
                        curr_child->cmd_str);
    }
  }

  // TODO --> FS people reduce reference count for all fds in the fd table?

  // delete this process from any queue it's in + free it
  delete_process_from_all_queues(proc);
  free_pcb(proc);
}
