/* CS5480 PennOS Group 61
 * Authors: Krystoff Purtell and Richard Zhang
 * Purpose: Implements the all scheduler-related functions and 
 *          initializes the scheduler queues.
 */

#include "scheduler.h"
#include <signal.h>  // TODO --> make sure this is ok to include
#include <stdbool.h>
#include <sys/time.h>
#include "../lib/Vec.h"
#include "../lib/spthread.h"
#include "errno.h"
#include "kern_pcb.h"
#include "logger.h"
#include "stdlib.h"

#include <stdio.h>  // TODO: delete this once finished
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////
//                       QUEUES AND SCHEDULER DATA                             //
/////////////////////////////////////////////////////////////////////////////////

Vec zero_priority_queue;  // lower index = more recent
Vec one_priority_queue;
Vec two_priority_queue;
Vec zombie_queue;
Vec sleep_blocked_queue;

Vec current_pcbs;  // holds all currently running processes, for logging

static const int hundred_millisec = 100000;  // 100 milliseconds
static bool scheduling_done = false;         // true if the scheduler is done

int tick_counter = 0;
int log_fd;  // file descriptor for the log file, set in pennos.c

pcb_t* current_running_pcb;  // currently running process

int curr_priority_arr_index = 0;
int det_priorities_arr[19] = {0, 1, 2, 0, 0, 1, 0, 1, 2, 0,
                              0, 1, 2, 0, 1, 0, 0, 1, 2};

/////////////////////////////////////////////////////////////////////////////////
//                         QUEUE MAINTENANCE FUNCTIONS                         //
/////////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the scheduler queues.
 *
 * Note: The deconstructors for the queues are set to NULL to prevent double
  *       freeing when exiting PennOS.
 */
void initialize_scheduler_queues() {
  zero_priority_queue = vec_new(0, NULL);
  one_priority_queue = vec_new(0, NULL);
  two_priority_queue = vec_new(0, NULL);
  zombie_queue = vec_new(0, NULL);
  sleep_blocked_queue = vec_new(0, NULL);
  current_pcbs = vec_new(0, free_pcb);
}

/**
 * Frees the scheduler queues.
 */
void free_scheduler_queues() {
  vec_destroy(&zero_priority_queue);
  vec_destroy(&one_priority_queue);
  vec_destroy(&two_priority_queue);
  vec_destroy(&zombie_queue);
  vec_destroy(&sleep_blocked_queue);
  vec_destroy(&current_pcbs);
}

/////////////////////////////////////////////////////////////////////////////////
//                          SCHEDULING FUNCTIONS                               //
/////////////////////////////////////////////////////////////////////////////////

/**
 * Generates the next priority for scheduling based on the defined probabilities.
 */
int generate_next_priority() {
  // check if all queues are empty
  if (vec_is_empty(&zero_priority_queue) && vec_is_empty(&one_priority_queue) &&
      vec_is_empty(&two_priority_queue)) {
    return -1;
  }

  int priorities_attempted = 0;
  while (priorities_attempted < 19) {
    int curr_pri = det_priorities_arr[curr_priority_arr_index];
    curr_priority_arr_index = (curr_priority_arr_index + 1) % 19;
    if (curr_pri == 0 && !vec_is_empty(&zero_priority_queue)) {
      priorities_attempted++;
      return 0;
    } else if (curr_pri == 1 && !vec_is_empty(&one_priority_queue)) {
      priorities_attempted++;
      return 1;
    } else if (curr_pri == 2 && !vec_is_empty(&two_priority_queue)) {
      priorities_attempted++;
      return 2;
    }
  }

  return -1;  // should never reach
}

/**
 * Gets the next PCB from the specified priority queue.
 */
pcb_t* get_next_pcb(int priority) {
  if (priority == -1) {  // all queues empty
    return NULL;
  }

  pcb_t* next_pcb = NULL;
  if (priority == 0) {
    next_pcb = vec_get(&zero_priority_queue, 0);
    vec_erase_no_deletor(&zero_priority_queue, 0);
  } else if (priority == 1) {
    next_pcb = vec_get(&one_priority_queue, 0);
    vec_erase_no_deletor(&one_priority_queue, 0);
  } else if (priority == 2) {
    next_pcb = vec_get(&two_priority_queue, 0);
    vec_erase_no_deletor(&two_priority_queue, 0);
  }

  return next_pcb;
}

/**
 * Puts the given PCB into the correct queue based on its priority and state.
 */
void put_pcb_into_correct_queue(pcb_t* pcb) {
  if (pcb->process_state == 'R') {
    if (pcb->priority == 0) {
      vec_push_back(&zero_priority_queue, pcb);
    } else if (pcb->priority == 1) {
      vec_push_back(&one_priority_queue, pcb);
    } else if (pcb->priority == 2) {
      vec_push_back(&two_priority_queue, pcb);
    }
  } else if (pcb->process_state == 'Z') {
    vec_push_back(&zombie_queue, pcb);
  } else if (pcb->process_state == 'B' || pcb->is_sleeping) {
    vec_push_back(&sleep_blocked_queue, pcb);
  }
}

/**
 * Deletes the given PCB from the specified queue.
 */
void delete_process_from_particular_queue(pcb_t* pcb, Vec* queue) {
  for (int i = 0; i < vec_len(queue); i++) {
    pcb_t* curr_pcb = vec_get(queue, i);
    if (curr_pcb->pid == pcb->pid) {
      vec_erase_no_deletor(queue, i);
      return;
    }
  }
}

/**
 * Deletes the given PCB from all queues except the current one.
 */
void delete_process_from_all_queues_except_current(pcb_t* pcb) {
  delete_process_from_particular_queue(pcb, &zero_priority_queue);
  delete_process_from_particular_queue(pcb, &one_priority_queue);
  delete_process_from_particular_queue(pcb, &two_priority_queue);
  delete_process_from_particular_queue(pcb, &zombie_queue);
  delete_process_from_particular_queue(pcb, &sleep_blocked_queue);
}

/**
 * Deletes the given PCB from all queues.
 */
void delete_process_from_all_queues(pcb_t* pcb) {
  delete_process_from_all_queues_except_current(pcb);
  delete_process_from_particular_queue(pcb, &current_pcbs);
}

/**
 * Gets the PCB with the specified PID from the given queue.
 */
pcb_t* get_pcb_in_queue(Vec* queue, pid_t pid) {
  for (int i = 0; i < vec_len(queue); i++) {
    pcb_t* curr_pcb = vec_get(queue, i);
    if (curr_pcb->pid == pid) {
      return curr_pcb;
    }
  }

  return NULL;
}

/**
 * Checks if the given parent PCB has any children in the zombie queue.
 */
bool child_in_zombie_queue(pcb_t* parent) {
  for (int i = 0; i < vec_len(&zombie_queue); i++) {
    pcb_t* child = vec_get(&zombie_queue, i);
    if (child->par_pid == parent->pid) {
      return true;
    }
  }
  return false;
}

/**
 * Checks if the given parent PCB has any children with a changed process status.
 */
bool child_with_changed_process_status(pcb_t* parent) {
  for (int i = 0; i < vec_len(&current_pcbs); i++) {
    pcb_t* child = vec_get(&current_pcbs, i);
    if (child->par_pid == parent->pid && child->process_status != 0) {
      return true;
    }
  }
  return false;
}

/**
 * Signal handler for SIGALRM.
 */
void alarm_handler(int signum) {
  tick_counter++;
}

/**
 * Handles the specified signal for the given PCB.
 */
void handle_signal(pcb_t* pcb, int signal) {
  switch (signal) {
    case 0:  // P_SIGSTOP
      if (pcb->process_state == 'R' || pcb->process_state == 'B') {
        pcb->process_state = 'S';
        log_generic_event('s', pcb->pid, pcb->priority, pcb->cmd_str);
        delete_process_from_all_queues_except_current(pcb);
        pcb->process_status = 21;  // STOPPED_BY_SIG
      }
      pcb->signals[0] = false;
      break;
    case 1:                             // P_SIGCONT
      if (pcb->process_state == 'S') {  // Only continue if stopped
        pcb->process_state = 'R';
        log_generic_event('c', pcb->pid, pcb->priority, pcb->cmd_str);
        delete_process_from_all_queues_except_current(pcb);
        put_pcb_into_correct_queue(pcb);
        pcb->process_status = 23;  // Reset status
      }
      pcb->signals[1] = false;
      break;
    case 2:                             // P_SIGTERM
      if (pcb->process_state != 'Z') {  // Don't terminate if already zombie
        pcb->process_state = 'Z';
        pcb->process_status = 22;  // TERM_BY_SIG
        log_generic_event('Z', pcb->pid, pcb->priority, pcb->cmd_str);
        delete_process_from_all_queues_except_current(pcb);
        put_pcb_into_correct_queue(pcb);
        pcb->process_status = 22;  // TERM_BY_SIG
      }
      pcb->signals[2] = false;
      break;
  }
}

/**
 * Shuts down the scheduler and cleans up resources.
 */
void s_shutdown_pennos(void) {
  scheduling_done = true;
}

/**
 * The main scheduler function for PennOS.
 */
void scheduler() {
  int curr_priority_queue_num;

  // mask for while scheduler is waiting for alarm
  sigset_t suspend_set;
  sigfillset(&suspend_set);
  sigdelset(&suspend_set, SIGALRM);

  // ensure sigarlm doesn't terminate the process
  struct sigaction act = (struct sigaction){
      .sa_handler = alarm_handler,
      .sa_mask = suspend_set,
      .sa_flags = SA_RESTART,
  };
  sigaction(SIGALRM, &act, NULL);

  // make sure SIGALRM is unblocked
  sigset_t alarm_set;
  sigemptyset(&alarm_set);
  sigaddset(&alarm_set, SIGALRM);
  pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);

  struct itimerval it;
  it.it_interval = (struct timeval){.tv_usec = hundred_millisec};
  it.it_value = it.it_interval;
  setitimer(ITIMER_REAL, &it, NULL);

  while (!scheduling_done) {
    // handle signals for the currently running process
    if (current_running_pcb != NULL) {
      for (int i = 0; i < 3; i++) {
        if (current_running_pcb->signals[i]) {
          handle_signal(current_running_pcb, i);
          // If process was terminated, don't continue scheduling it
          if (current_running_pcb->process_state != 'R') {
            current_running_pcb = NULL;
            break;
          }
        }
      }
    }

    // handle signals for all other processes (currently running or not)
    for (int i = 0; i < vec_len(&current_pcbs); i++) {
      pcb_t* curr_pcb = vec_get(&current_pcbs, i);
      for (int j = 0; j < 3; j++) {
        if (curr_pcb->signals[j]) {
          handle_signal(curr_pcb, j);
        }
      }
    }

    // Check sleep/blocked queue to move processes back to scheduable queues
    for (int i = 0; i < vec_len(&sleep_blocked_queue); i++) {
      pcb_t* blocked_proc = vec_get(&sleep_blocked_queue, i);
      bool make_runnable = false;
      if (blocked_proc->is_sleeping &&
          blocked_proc->time_to_wake == tick_counter) {
        blocked_proc->is_sleeping = false;
        blocked_proc->time_to_wake = -1;
        blocked_proc->signals[2] = false;  // Unlikely, but reset signal
        make_runnable = true;
      }  // TODO: THIS BLOCK MAY BE REDUNDANT NOW B/C SIGNAL HANDLERS CALLED
      else if (blocked_proc->is_sleeping &&
               blocked_proc->signals[2]) {  // P_SIGTERM received
        blocked_proc->is_sleeping = false;
        blocked_proc->process_state = 'Z';
        blocked_proc->process_status = 22;  // TERM_BY_SIG
        blocked_proc->signals[2] = false;
        delete_process_from_all_queues_except_current(blocked_proc);
        put_pcb_into_correct_queue(blocked_proc);
        log_generic_event('Z', blocked_proc->pid, blocked_proc->priority,
                          blocked_proc->cmd_str);
        i--;
      } else if (child_in_zombie_queue(blocked_proc)) {
        make_runnable = true;
      } else if (child_with_changed_process_status(blocked_proc)) {
        make_runnable = true;
      }

      if (make_runnable) {
        blocked_proc->process_state = 'R';
        vec_erase_no_deletor(&sleep_blocked_queue, i);
        delete_process_from_all_queues_except_current(blocked_proc);
        put_pcb_into_correct_queue(blocked_proc);
        log_generic_event('U', blocked_proc->pid, blocked_proc->priority,
                          blocked_proc->cmd_str);
        i--;
      }
    }

    curr_priority_queue_num = generate_next_priority();

    current_running_pcb = get_next_pcb(curr_priority_queue_num);
    if (current_running_pcb == NULL) {
      sigsuspend(&suspend_set);  // idle until signal received
      continue;
    }

    log_scheduling_event(current_running_pcb->pid, curr_priority_queue_num,
                         current_running_pcb->cmd_str);

    if (spthread_continue(current_running_pcb->thread_handle) != 0 &&
        errno != EINTR) {
      perror("spthread_continue failed in scheduler");
    }
    sigsuspend(&suspend_set);
    if (spthread_suspend(current_running_pcb->thread_handle) != 0 &&
        errno != EINTR) {
      perror("spthread_suspend failed in scheduler");
    }
    put_pcb_into_correct_queue(current_running_pcb);
  }
}
