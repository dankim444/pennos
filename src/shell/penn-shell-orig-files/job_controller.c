/* CS5480 Project 02 penn-shell
 * Authors: Dan Kim and Richard Zhang
 * Purpose: Job controller helpers.
 */

#include "job_controller.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "Job.h"
#include "Vec.h"
#include "parser.h"
#include "signals.h"

extern Vec job_list;
extern jid_t next_job_id;
extern bool interactive_mode;

// This function will be used by vec_new as the destructor
// for each job pointer.
void free_job_ptr(void* ptr) {
  job* job_ptr = (job*)ptr;
  free(job_ptr->pids);
  free(job_ptr->cmd);
  free(job_ptr);
}

// Initializes the job list vector
void init_job_vector(void) {
  job_list = vec_new(10, free_job_ptr);
}

// helper function for printing a command
void print_job_status_message(const char* status,
                              const struct parsed_command* cmd) {
  fprintf(stderr, "%s", status);

  for (size_t i = 0; i < cmd->num_commands; ++i) {
    for (char** arguments = cmd->commands[i]; *arguments != NULL; ++arguments) {
      fprintf(stderr, "%s ", *arguments);
    }

    if (i == 0 && cmd->stdin_file != NULL) {
      fprintf(stderr, "< %s ", cmd->stdin_file);
    }

    if (i == cmd->num_commands - 1 && cmd->stdout_file != NULL) {
      fprintf(stderr, cmd->is_file_append ? ">> %s " : "> %s ",
              cmd->stdout_file);
    } else if (i < cmd->num_commands - 1) {
      fprintf(stderr, "| ");
    }
  }

  fprintf(stderr, "\n");
}

job* findJobByIdOrCurrent(const char* arg) {
  if (vec_len(&job_list) == 0) {
    return NULL;
  }

  if (arg != NULL) {
    // parse numeric
    char* endPtr = NULL;
    long val = strtol(arg, &endPtr, 10);
    if (*endPtr != '\0' || val < 1) {
      return NULL;
    }
    for (size_t i = 0; i < vec_len(&job_list); i++) {
      job* job_ptr = (job*)vec_get(&job_list, i);
      if ((jid_t)val == job_ptr->id) {
        return job_ptr;
      }
    }
    return NULL;
  }

  // Look for most recently stopped job first
  for (size_t i = vec_len(&job_list); i > 0; i--) {
    job* job_ptr = (job*)vec_get(&job_list, i - 1);
    if (job_ptr->state == STOPPED) {
      return job_ptr;
    }
  }

  return (job*)vec_get(&job_list, vec_len(&job_list) - 1);
}

void builtinJobs(void) {
  if (vec_is_empty(&job_list)) {
    return;
  }

  for (size_t idx = 0; idx < vec_len(&job_list); idx++) {
    job* job_ptr = (job*)vec_get(&job_list, idx);

    const char* state = "unknown";
    if (job_ptr->state == RUNNING) {
      state = "running";
    } else if (job_ptr->state == STOPPED) {
      state = "stopped";
    } else if (job_ptr->state == FINISHED) {
      state = "finished";
    }

    fprintf(stderr, "[%lu] ", (unsigned long)job_ptr->id);
    // print pipeline
    for (size_t cmdIdx = 0; cmdIdx < job_ptr->cmd->num_commands; cmdIdx++) {
      char** argv = job_ptr->cmd->commands[cmdIdx];
      int argIdx = 0;
      while (argv[argIdx] != NULL) {
        fprintf(stderr, "%s ", argv[argIdx]);
        argIdx++;
      }
      if (cmdIdx < job_ptr->cmd->num_commands - 1) {
        fprintf(stderr, "| ");
      }
    }
    fprintf(stderr, "(%s)\n", state);
  }
}

void builtinBg(const char* jobArg) {
  job* job_ptr = findJobByIdOrCurrent(jobArg);
  if (!job_ptr) {
    fprintf(stderr, "bg: no such job\n");
    return;
  }
  if (job_ptr->state == STOPPED) {
    job_ptr->state = RUNNING;
    fprintf(stderr, "Running: ");
    // print pipeline
    for (size_t cmdIdx = 0; cmdIdx < job_ptr->cmd->num_commands; cmdIdx++) {
      char** argv = job_ptr->cmd->commands[cmdIdx];
      int argIdx = 0;
      while (argv[argIdx] != NULL) {
        fprintf(stderr, "%s ", argv[argIdx]);
        argIdx++;
      }
      if (cmdIdx < job_ptr->cmd->num_commands - 1) {
        fprintf(stderr, "| ");
      }
    }
    fprintf(stderr, "\n");
    kill(-job_ptr->pgid, SIGCONT);
  } else if (job_ptr->state == RUNNING) {
    fprintf(stderr, "bg: job [%lu] is already running\n",
            (unsigned long)job_ptr->id);
  } else {
    fprintf(stderr, "bg: job [%lu] not stopped\n", (unsigned long)job_ptr->id);
  }
}

void builtinFg(const char* jobArg) {
  job* job_ptr = findJobByIdOrCurrent(jobArg);
  if (!job_ptr) {
    fprintf(stderr, "fg: no such job\n");
    return;
  }

  pid_t shellPgid = getpgrp();

  if (job_ptr->state == FINISHED) {
    fprintf(stderr, "fg: job [%lu] is already finished\n",
            (unsigned long)job_ptr->id);
    return;
  }

  if (job_ptr->state == STOPPED) {
    job_ptr->state = RUNNING;
    fprintf(stderr, "Restarting: ");
    for (size_t cmdIdx = 0; cmdIdx < job_ptr->cmd->num_commands; cmdIdx++) {
      char** argv = job_ptr->cmd->commands[cmdIdx];
      int argIdx = 0;
      while (argv[argIdx] != NULL) {
        fprintf(stderr, "%s ", argv[argIdx]);
        argIdx++;
      }
      if (cmdIdx < job_ptr->cmd->num_commands - 1) {
        fprintf(stderr, "| ");
      }
    }
    fprintf(stderr, "\n");
    kill(-job_ptr->pgid, SIGCONT);
  } else {
    // it's running in background => bring to foreground
    for (size_t cmdIdx = 0; cmdIdx < job_ptr->cmd->num_commands; cmdIdx++) {
      char** argv = job_ptr->cmd->commands[cmdIdx];
      int argIdx = 0;
      while (argv[argIdx] != NULL) {
        fprintf(stderr, "%s ", argv[argIdx]);
        argIdx++;
      }
      if (cmdIdx < job_ptr->cmd->num_commands - 1) {
        fprintf(stderr, "| ");
      }
    }
    fprintf(stderr, "\n");
  }

  // Set this as the foreground job
  set_foreground_pgid(job_ptr->pgid);
  tcsetpgrp(STDIN_FILENO, job_ptr->pgid);

  while (true) {
    int status = 0;
    pid_t wpid = waitpid(-job_ptr->pgid, &status, WUNTRACED);
    if (wpid < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }
    if (WIFEXITED(status) || WIFSIGNALED(status)) {
      job_ptr->state = FINISHED;
      // Remove finished job from list
      for (size_t i = 0; i < vec_len(&job_list); i++) {
        if ((job*)vec_get(&job_list, i) == job_ptr) {
          vec_erase(&job_list, i);
          break;
        }
      }
      break;
    }
    if (WIFSTOPPED(status)) {
      job_ptr->state = STOPPED;
      fprintf(stderr, "Stopped: ");
      print_parsed_command(job_ptr->cmd);
      break;
    }
  }

  // Clear foreground pgid and return terminal to shell
  clear_foreground_pgid();
  tcsetpgrp(STDIN_FILENO, shellPgid);
}

// The main function that checks if parsed_result is "jobs", "fg", or "bg"
bool handle_builtin(struct parsed_command* parsed) {
  if (parsed->num_commands < 1) {
    return false;
  }
  if (parsed->commands[0] == NULL) {
    return false;
  }

  char** argv = parsed->commands[0];
  if (argv[0] == NULL) {
    return false;
  }

  if (strcmp(argv[0], "jobs") == 0) {
    builtinJobs();
    return true;
  }
  if (strcmp(argv[0], "bg") == 0) {
    builtinBg(argv[1]);
    return true;
  }
  if (strcmp(argv[0], "fg") == 0) {
    builtinFg(argv[1]);
    return true;
  }

  return false;
}
