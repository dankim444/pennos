/* CS5480 PennOS Group 61
 * Authors: Krystof Purtell and Richard Zhang
 * Purpose: Implements the process-related helper functions
 *          and the kernel-level process functions.  
 */

#include "kern_pcb.h"
#include "../fs/fs_helpers.h"
#include "../fs/fs_syscalls.h"
#include "../lib/pennos-errno.h"
#include "logger.h"
#include "scheduler.h"
#include "stdio.h"  // for perror
#include "stdlib.h"
#include "../shell/builtins.h"

int next_pid = 2;  // global variable to track the next pid to be assigned
                   // Note: Starts at 2 because init is 1

extern Vec current_pcbs;
extern pcb_t* current_running_pcb;

////////////////////////////////////////////////////////////////////////////////
//                              PCB FUNCTIONS                                 //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Free resources associated with a PCB.
 */
void free_pcb(void* pcb) {
  pcb_t* casted_pcb = (pcb_t*)pcb;

  free(casted_pcb->cmd_str);
  vec_destroy(&casted_pcb->child_pcbs);  // will free any remaining
                                         // children too!
  free(casted_pcb);
}

/**
 * @brief Initializes a PCB with the given parameters.
 */
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

  ret_pcb->child_pcbs = vec_new(0, NULL); // NULL deconstructor prevents
                                          // double free

  for (int i = 0; i < 3; i++) {
    ret_pcb->signals[i] = false;
  }

  ret_pcb->is_sleeping = false;
  ret_pcb->time_to_wake = -1;  // default to not sleeping

  return ret_pcb;
}

/**
 * @brief Removes a child PCB from its parent's child list.
 */
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
//           KERNEL-LEVEl PROCESS-RELATED REQUIRED KERNEL FUNCTIONS           //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Creates a new process. If the parent is NULL, it creates the init process.
 */
pcb_t* k_proc_create(pcb_t* parent, int priority) {
  if (parent == NULL) {  // init creation case
    pcb_t* init = create_pcb(1, 0, 0, 0, 1);
    if (init == NULL) {
      P_ERRNO = P_ENULL;
      return NULL;
    }
    init->fd_table[0] = STDIN_FILENO;
    init->fd_table[1] = STDOUT_FILENO;
    init->fd_table[2] = STDERR_FILENO;
    for (int i = 3; i < FILE_DESCRIPTOR_TABLE_SIZE; i++) {
      init->fd_table[i] = -1;
    }

    increment_fd_ref_count(STDIN_FILENO);
    increment_fd_ref_count(STDOUT_FILENO);
    increment_fd_ref_count(STDERR_FILENO);

    current_running_pcb = init;
    put_pcb_into_correct_queue(init);
    vec_push_back(&current_pcbs, init);
    return init;
  }

  pcb_t* child = create_pcb(next_pid++, parent->pid, priority, parent->input_fd,
                            parent->output_fd);
  if (child == NULL) {
    P_ERRNO = P_ENULL;
    return NULL;
  }

  // copy parent's fd table
  for (int i = 0; i < FILE_DESCRIPTOR_TABLE_SIZE; i++) {
    child->fd_table[i] = parent->fd_table[i];
  }

  for (int i = 0; i < FILE_DESCRIPTOR_TABLE_SIZE; i++) {
    if (child->fd_table[i] != -1) {
      increment_fd_ref_count(child->fd_table[i]);
    }
  }

  // update parent as needed
  vec_push_back(&parent->child_pcbs, child);

  // add to appropriate queue
  put_pcb_into_correct_queue(child);
  vec_push_back(&current_pcbs, child);

  return child;
}

/**
 * @brief Cleans up a process by removing it from its parent's child list,
 * removing its children, decrementing file descriptor reference counts,
 * closing files, and freeing the PCB.
 */
void k_proc_cleanup(pcb_t* proc) {
  // if proc has parent (i.e. isn't init) then remove it from parent's child
  // list
  pcb_t* par_pcb = get_pcb_in_queue(&current_pcbs, proc->par_pid);
  if (par_pcb != NULL) {
    remove_child_in_parent(par_pcb, proc);
  } else {
    P_ERRNO = P_ENULL;
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

  // decr reference counts + close files if necessary
  for (int i = 0; i < FILE_DESCRIPTOR_TABLE_SIZE; i++) {
    if (proc->fd_table[i] != -1) {
      if (decrement_fd_ref_count(proc->fd_table[i]) == 0) {
        if (s_close(proc->fd_table[i]) == -1) {
          u_perror("closing on a non-valid fd");
        }
      }
    }
  }

  // cancel + join this thread
  spthread_cancel(proc->thread_handle);
  spthread_continue(proc->thread_handle);
  spthread_suspend(proc->thread_handle);
  spthread_join(proc->thread_handle, NULL);

  // delete this process from any queue it's in + free it
  delete_process_from_all_queues(proc);
  free_pcb(proc);
}
