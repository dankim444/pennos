/* CS5480 Project 02 penn-shell
 * Authors: Dan Kim and Richard Zhang
 * Purpose: Code for polling background jobs.
 */

#include "poll.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Job.h"
#include "Vec.h"
#include "job_controller.h"
#include "parser.h"

extern Vec job_list;  // from penn-shell.c

// We increment job->finished_count each time a child is "done".
static void mark_pid_finished(job* job, pid_t cpid) {
  job->finished_count++;
}

// Check if job->finished_count == job->num_pids
static bool job_is_completely_done(const job* job) {
  return (job->finished_count == job->num_pids);
}

void poll_background_jobs(void) {
  int status;
  pid_t cpid;

  // Reap as many changed children as possible
  while ((cpid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
    // Find which job cpid belongs to
    for (size_t i = 0; i < vec_len(&job_list); i++) {
      job* job = vec_get(&job_list, i);
      bool in_this_job = false;
      for (size_t j = 0; j < job->num_pids; j++) {
        if (job->pids[j] == cpid) {
          in_this_job = true;
          break;
        }
      }

      if (!in_this_job) {
        continue;
      }

      // If the process ended normally or via signal
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
        mark_pid_finished(job, cpid);
        if (job_is_completely_done(job)) {
          print_job_status_message("Finished: ", job->cmd);
          vec_erase(&job_list, i);
        }
      } else if (WIFSTOPPED(status) && job->state == RUNNING) {
        job->state = STOPPED;
        print_job_status_message("Stopped: ", job->cmd);
      }

      break;  // break from for-loop over job_list
    }
  }
}
