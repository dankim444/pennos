#ifndef JOB_H_
#define JOB_H_

#include <stdint.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "./parser.h"

// define new type "job id"
typedef uint64_t jid_t;

// define job states
typedef enum { RUNNING, STOPPED, FINISHED } job_state_t;

// Represents a job
typedef struct job_st {
  jid_t id;
  struct parsed_command* cmd;
  pid_t* pids;  // array of pids of size cmd->num_commands
  job_state_t state;
  size_t num_pids;
  pid_t pgid;
  size_t finished_count;  // tracks how many pids have exited
} job;

#endif  // JOB_H_
