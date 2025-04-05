/* CS5480 Project 02 penn-shell
 * Authors: Dan Kim and Richard Zhang
 * Purpose: Implements pipeline creation and foreground/background job handling.
 */

#include "executor.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Job.h"
#include "Vec.h"
#include "job_controller.h"
#include "parser.h"
#include "signals.h"

#define perm 0644

extern Vec job_list;       // from penn-shell.c
extern jid_t next_job_id;  // from penn-shell.c

static struct sigaction oldSigint;
static struct sigaction oldSigtstp;

void ignore_signals_during_fork(void) {
  struct sigaction ignoreAct;
  ignoreAct.sa_handler = SIG_IGN;
  ignoreAct.sa_flags = 0;
  sigemptyset(&ignoreAct.sa_mask);

  // Temporarily ignore SIGINT
  if (sigaction(SIGINT, &ignoreAct, &oldSigint) < 0) {
    perror("sigaction(SIGINT => SIG_IGN)");
    exit(EXIT_FAILURE);
  }
  // Temporarily ignore SIGTSTP
  if (sigaction(SIGTSTP, &ignoreAct, &oldSigtstp) < 0) {
    perror("sigaction(SIGTSTP => SIG_IGN)");
    exit(EXIT_FAILURE);
  }
}

void restore_signals_after_fork(void) {
  // Restore SIGINT
  if (sigaction(SIGINT, &oldSigint, NULL) < 0) {
    perror("sigaction(SIGINT => oldSigint)");
    exit(EXIT_FAILURE);
  }
  // Restore SIGTSTP
  if (sigaction(SIGTSTP, &oldSigtstp, NULL) < 0) {
    perror("sigaction(SIGTSTP => oldSigtstp)");
    exit(EXIT_FAILURE);
  }
}

void create_pipes(size_t pipeCount, int* pipeFds) {
  for (size_t idx = 0; idx < pipeCount; idx++) {
    if (pipe(pipeFds + idx * 2) == -1) {
      perror("pipe function error");
      exit(EXIT_FAILURE);
    }
  }
}

void close_parent_pipes(size_t pipeCount, int* pipeFds) {
  for (int closeIdx = 0; closeIdx < (int)(2 * pipeCount); closeIdx++) {
    close(pipeFds[closeIdx]);
  }
}

void spawn_pipeline_children(struct parsed_command* parsedResult,
                             size_t commands_count,
                             int* pipeFds,
                             pid_t* pids,
                             pid_t* pgidPtr) {
  pid_t pgidLocal = 0;
  size_t pipeCount = commands_count - 1;

  for (int cmdIndex = 0; cmdIndex < (int)commands_count; cmdIndex++) {
    pid_t cpid = fork();
    if (cpid == -1) {
      perror("fork function error");
      exit(EXIT_FAILURE);
    }

    if (cpid == 0) {
      // Child process
      restore_default_signals();

      // set PGID
      if (cmdIndex == 0) {
        // first child determines pgid
        setpgid(0, 0);
      } else {
        setpgid(0, pgidLocal);
      }

      // handle redirection for the first/last command
      if (cmdIndex == 0 && parsedResult->stdin_file != NULL) {
        int fdIn = open(parsedResult->stdin_file, O_RDONLY);
        if (fdIn == -1) {
          perror("open function stdin_file error");
          exit(EXIT_FAILURE);
        }
        if (dup2(fdIn, STDIN_FILENO) == -1) {
          perror("dup2 function redirect stdin");
          exit(EXIT_FAILURE);
        }
        close(fdIn);
      }
      if ((size_t)cmdIndex == commands_count - 1 &&
          parsedResult->stdout_file != NULL) {
        int fdOut = -1;
        if (parsedResult->is_file_append) {
          fdOut = open(parsedResult->stdout_file, O_WRONLY | O_CREAT | O_APPEND,
                       perm);
        } else {
          fdOut = open(parsedResult->stdout_file, O_WRONLY | O_CREAT | O_TRUNC,
                       perm);
        }
        if (fdOut == -1) {
          perror("open function stdout_file error");
          exit(EXIT_FAILURE);
        }
        if (dup2(fdOut, STDOUT_FILENO) == -1) {
          perror("dup2 function redirect stdout");
          exit(EXIT_FAILURE);
        }
        close(fdOut);
      }

      // chain the pipes
      if (cmdIndex > 0) {
        if (dup2(pipeFds[2 * cmdIndex - 2], STDIN_FILENO) == -1) {
          perror("dup2 read end");
          exit(EXIT_FAILURE);
        }
      }
      if ((size_t)cmdIndex < commands_count - 1) {
        if (dup2(pipeFds[2 * cmdIndex + 1], STDOUT_FILENO) == -1) {
          perror("dup2 write end");
          exit(EXIT_FAILURE);
        }
      }

      // close all pipe ends
      for (int closeIdx = 0; closeIdx < (int)(2 * pipeCount); closeIdx++) {
        close(pipeFds[closeIdx]);
      }

      // execute command
      execvp(parsedResult->commands[cmdIndex][0],
             parsedResult->commands[cmdIndex]);
      perror("execvp error with parsed command");
      exit(EXIT_FAILURE);
    }

    // parent
    pids[cmdIndex] = cpid;
    if (cmdIndex == 0) {
      pgidLocal = cpid;
      setpgid(cpid, cpid);
    } else {
      setpgid(cpid, pgidLocal);
    }
  }
  *pgidPtr = pgidLocal;
}

void handle_interactive_fg(struct parsed_command* parsedResult,
                           pid_t pgid,
                           pid_t* pids,
                           size_t commands_count) {
  // Set the foreground process group
  set_foreground_pgid(pgid);
  tcsetpgrp(STDIN_FILENO, pgid);

  // wait for each child
  bool job_stopped = false;

  for (int idx = 0; idx < (int)commands_count; idx++) {
    int status = 0;
    pid_t waited_pid;

    // Use a loop to handle EINTR (interrupted system call)
    do {
      waited_pid = waitpid(pids[idx], &status, WUNTRACED);
    } while (waited_pid == -1 && errno == EINTR);

    // Handle stopped processes
    if (waited_pid != -1 && WIFSTOPPED(status)) {
      // If any process in the pipeline is stopped, the whole job is stopped
      job_stopped = true;

      // Create a new job for the stopped foreground job
      job* stopped_job = malloc(sizeof(job));
      if (stopped_job == NULL) {
        perror("malloc stopped_job");
        exit(EXIT_FAILURE);
      }

      stopped_job->id = next_job_id++;
      stopped_job->pgid = pgid;
      stopped_job->num_pids = commands_count;
      stopped_job->pids = malloc(sizeof(pid_t) * commands_count);
      if (stopped_job->pids == NULL) {
        perror("malloc stopped_job->pids");
        exit(EXIT_FAILURE);
      }

      for (size_t i = 0; i < commands_count; i++) {
        stopped_job->pids[i] = pids[i];
      }

      stopped_job->state = STOPPED;
      stopped_job->cmd = parsedResult;
      stopped_job->finished_count = 0;

      // Add to job list first
      vec_push_back(&job_list, stopped_job);
      break;
    }
  }

  // Clear the foreground process group and give terminal back to shell
  clear_foreground_pgid();
  tcsetpgrp(STDIN_FILENO, getpid());
  //fprintf(stderr, "\n");

  // Only now print the stopped message if job was stopped
  if (job_stopped) {
    print_job_status_message("Stopped: ", parsedResult);
  }
}

void handle_interactive_bg(struct parsed_command* parsedResult,
                           pid_t pgid,
                           const pid_t* pids,
                           size_t commands_count) {
  print_job_status_message("Running: ", parsedResult);

  job* bgJob = malloc(sizeof(job));
  if (bgJob == NULL) {
    perror("malloc bgJob");
    exit(EXIT_FAILURE);
  }

  bgJob->id = next_job_id++;
  bgJob->pgid = pgid;
  bgJob->num_pids = commands_count;
  bgJob->pids = malloc(sizeof(pid_t) * commands_count);
  if (bgJob->pids == NULL) {
    perror("malloc bgJob->pids");
    exit(EXIT_FAILURE);
  }
  for (size_t idx = 0; idx < commands_count; idx++) {
    bgJob->pids[idx] = pids[idx];
  }
  bgJob->state = RUNNING;
  bgJob->cmd = parsedResult;
  bgJob->finished_count = 0;

  vec_push_back(&job_list, bgJob);
}

void execute(struct parsed_command* parsed_result, bool interactive_mode) {
  size_t commands_count = parsed_result->num_commands;
  if (commands_count == 0) {
    return;
  }

  ignore_signals_during_fork();

  size_t pipeCount = commands_count - 1;
  int* pipeFds = NULL;
  if (pipeCount > 0) {
    pipeFds = malloc(sizeof(int) * pipeCount * 2);
    if (pipeFds == NULL) {
      perror("malloc pipeFds");
      exit(EXIT_FAILURE);
    }
    create_pipes(pipeCount, pipeFds);
  }

  pid_t* pids = malloc(sizeof(pid_t) * commands_count);
  if (pids == NULL) {
    perror("malloc pids");
    exit(EXIT_FAILURE);
  }
  pid_t pgid = 0;

  // fork children in parallel
  spawn_pipeline_children(parsed_result, commands_count, pipeFds, pids, &pgid);

  // done forking, restore signals
  restore_signals_after_fork();

  // close parent pipes
  if (pipeCount > 0) {
    close_parent_pipes(pipeCount, pipeFds);
    free(pipeFds);
  }

  // handle FG/BG in interactive mode
  if (interactive_mode) {
    if (parsed_result->is_background) {
      handle_interactive_bg(parsed_result, pgid, pids, commands_count);
      free(pids);
      return;
    }
    handle_interactive_fg(parsed_result, pgid, pids, commands_count);
  } else {
    // non-interactive => always wait
    for (int idx = 0; idx < (int)commands_count; idx++) {
      int status = 0;
      waitpid(pids[idx], &status, 0);
    }
  }
  free(pids);
}
