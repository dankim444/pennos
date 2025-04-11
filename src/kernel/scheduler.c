#include "scheduler.h"
#include <signal.h>  // TODO --> make sure this is ok to include
#include <stdbool.h>
#include <stdio.h>  // TODO: delete this once finished
#include <sys/time.h>
#include "../lib/Vec.h"
#include "../lib/spthread.h"
#include "kern_pcb.h"
#include "logger.h"
#include "stdlib.h"

#include <stdio.h>  // TODO: delete this once finished
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////
//                       QUEUES AND SCHEDULER DATA //
/////////////////////////////////////////////////////////////////////////////////

Vec zero_priority_queue;  // lower index = more recent
Vec one_priority_queue;
Vec two_priority_queue;
Vec zombie_queue;
Vec sleep_blocked_queue;
Vec waiting_parents;

Vec current_pcbs;  // holds all currently running processes, for logging

static const int hundred_millisec = 100000;  // 100 milliseconds
static bool scheduling_done = false;         // true if the scheduler is done

int tick_counter = 0;
int log_fd;  // file descriptor for the log file, set in pennos.c

int curr_priority_arr_index = 0;
int det_priorities_arr[19] = {0, 1, 2, 0, 0, 1, 0, 1, 2, 0,
                              0, 1, 2, 0, 1, 0, 0, 1, 2};

pcb_t* current_running_pcb = NULL;  // currently running process

/////////////////////////////////////////////////////////////////////////////////
//                         QUEUE MAINTENANCE FUNCTIONS //
/////////////////////////////////////////////////////////////////////////////////

void initialize_scheduler_queues() {
  zero_priority_queue = vec_new(0, free_pcb);
  one_priority_queue = vec_new(0, free_pcb);
  two_priority_queue = vec_new(0, free_pcb);
  zombie_queue = vec_new(0, free_pcb);
  sleep_blocked_queue = vec_new(0, free_pcb);
  current_pcbs = vec_new(0, free_pcb);
  waiting_parents = vec_new(0, NULL);
}

void free_scheduler_queues() {
  vec_destroy(&zero_priority_queue);
  vec_destroy(&one_priority_queue);
  vec_destroy(&two_priority_queue);
  vec_destroy(&zombie_queue);
  vec_destroy(&sleep_blocked_queue);
  vec_destroy(&current_pcbs);
  vec_destroy(&waiting_parents);
}

/////////////////////////////////////////////////////////////////////////////////
//                         SCHEDULING FUNCTIONS //
/////////////////////////////////////////////////////////////////////////////////

int generate_next_priority() {
  bool priority_found = false;
  while (!priority_found) {
    int curr_pri = det_priorities_arr[curr_priority_arr_index];
    curr_priority_arr_index = (curr_priority_arr_index + 1) % 19;
    if (curr_pri == 0 && !vec_is_empty(&zero_priority_queue)) {
      priority_found = true;
      return 0;
    } else if (curr_pri == 1 && !vec_is_empty(&one_priority_queue)) {
      priority_found = true;
      return 1;
    } else if (curr_pri == 2 && !vec_is_empty(&two_priority_queue)) {
      priority_found = true;
      return 2;
    }
  }

  return -1;  // should never reach here by assumption
}

pcb_t* get_next_pcb(int priority) {
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
  } else if (pcb->process_state == 'B' || pcb->process_state == 'S') {
    vec_push_back(&sleep_blocked_queue, pcb);
  }
}

void delete_process_from_particular_queue(pcb_t* pcb, Vec* queue) {
  for (int i = 0; i < vec_len(queue); i++) {
    pcb_t* curr_pcb = vec_get(queue, i);
    if (curr_pcb->pid == pcb->pid) {
      vec_erase_no_deletor(queue, i);
      return;
    }
  }
}

void delete_process_from_all_queues(pcb_t* pcb) {
  delete_process_from_particular_queue(pcb, &zero_priority_queue);
  delete_process_from_particular_queue(pcb, &one_priority_queue);
  delete_process_from_particular_queue(pcb, &two_priority_queue);
  delete_process_from_particular_queue(pcb, &zombie_queue);
  delete_process_from_particular_queue(pcb, &sleep_blocked_queue);
  delete_process_from_particular_queue(pcb, &current_pcbs);
}

pcb_t* get_pcb_in_queue(Vec* queue, pid_t pid) {
  for (int i = 0; i < vec_len(queue); i++) {
    pcb_t* curr_pcb = vec_get(queue, i);
    if (curr_pcb->pid == pid) {
      return curr_pcb;
    }
  }

  return NULL;
}

void alarm_handler(int signum) {
  tick_counter++;
}

void handle_signal(pcb_t* pcb, int signal) {
  switch (signal) {
    case 0:                             // P_SIGSTOP
      if (pcb->process_state == 'R') {  // Only stop if running
        pcb->process_state = 'S';
        log_generic_event('s', pcb->pid, pcb->priority, pcb->cmd_str);
        // Remove from current queue and add to blocked queue
        delete_process_from_all_queues(pcb);
        vec_push_back(&sleep_blocked_queue, pcb);
      }
      break;
    case 1:                             // P_SIGCONT
      if (pcb->process_state == 'S') {  // Only continue if stopped
        pcb->process_state = 'R';
        log_generic_event('c', pcb->pid, pcb->priority, pcb->cmd_str);
        // Remove from blocked queue and add to appropriate priority queue
        delete_process_from_all_queues(pcb);
        put_pcb_into_correct_queue(pcb);
      }
      break;
    case 2:                             // P_SIGTERM
      if (pcb->process_state != 'Z') {  // Don't terminate if already zombie
        pcb->process_state = 'Z';
        pcb->process_status = 22;  // TERM_BY_SIG
        log_generic_event('S', pcb->pid, pcb->priority, pcb->cmd_str);
        // Remove from current queue and add to zombie queue
        delete_process_from_all_queues(pcb);
        vec_push_back(&zombie_queue, pcb);

        // Handle orphaned children
        for (int i = 0; i < vec_len(&pcb->child_pcbs); i++) {
          pcb_t* child = vec_get(&pcb->child_pcbs, i);
          child->par_pid = 1;  // Set parent to init
          log_generic_event('O', child->pid, child->priority, child->cmd_str);
        }
      }
      break;
  }
}

// TODO --> this function needs a solid amt of work
void scheduler() {
  // Set up signal handling for SIGALRM
  sigset_t suspend_set;
  sigfillset(&suspend_set);
  sigdelset(&suspend_set, SIGALRM);

  struct sigaction act = (struct sigaction){
      .sa_handler = alarm_handler,
      .sa_mask = suspend_set,
      .sa_flags = SA_RESTART,
  };
  sigaction(SIGALRM, &act, NULL);

  // Ensure SIGALRM is unblocked
  sigset_t alarm_set;
  sigemptyset(&alarm_set);
  sigaddset(&alarm_set, SIGALRM);
  pthread_sigmask(SIG_UNBLOCK, &alarm_set, NULL);

  // Set up timer for 100ms intervals
  struct itimerval it;
  it.it_interval = (struct timeval){.tv_usec = hundred_millisec};
  it.it_value = it.it_interval;
  setitimer(ITIMER_REAL, &it, NULL);

  while (!scheduling_done) {
    // Check for signals on current process
    if (current_running_pcb != NULL) {
      for (int i = 0; i < 3; i++) {
        if (current_running_pcb->signals[i]) {
          handle_signal(current_running_pcb, i);
          current_running_pcb->signals[i] = false;

          // If process was terminated, don't continue scheduling it
          if (current_running_pcb->process_state == 'Z') {
            current_running_pcb = NULL;
            break;
          }
        }
      }
    }

    // Get next process to run
    int next_priority = generate_next_priority();

    if (next_priority == -1) {
      // No runnable processes, go to idle state
      // Log scheduler as blocked during idle
      log_generic_event('B', 0, 0, "scheduler");
      sigsuspend(&suspend_set);
      continue;
    }

    pcb_t* next_pcb = get_next_pcb(next_priority);
    if (next_pcb == NULL) {
      continue;
    }

    // Log scheduling decision
    log_scheduling_event(next_pcb->pid, next_pcb->priority, next_pcb->cmd_str);

    // Update current running process
    current_running_pcb = next_pcb;
    next_pcb->process_state = 'R';

    // Run the process
    spthread_continue(next_pcb->thread_handle);

    // Wait for next tick
    sigsuspend(&suspend_set);

    // Suspend the process
    spthread_suspend(next_pcb->thread_handle);

    // Put process back in appropriate queue if it's still runnable
    if (next_pcb->process_state == 'R') {
      put_pcb_into_correct_queue(next_pcb);
    }

    // Clear current running process if it's the one we just suspended
    if (current_running_pcb == next_pcb) {
      current_running_pcb = NULL;
    }
  }
}
