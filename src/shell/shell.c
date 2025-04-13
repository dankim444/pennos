#include <string.h>
#include "../fs/fs_syscalls.h"
#include "../kernel/kern_sys_calls.h"
#include "builtins.h"  // contains u_perrror
#include "parser.h"
#include "shell-built-ins.h"
#include "stdlib.h"

#include <fcntl.h>
#include <unistd.h>  // delete these once finished
#include "../kernel/scheduler.h"
#include "../lib/Vec.h"
#include "Job.h"
#include "signal.h"

#ifndef PROMPT
#define PROMPT "$ "
#endif

#define MAX_BUFFER_SIZE 4096

// Global variable to track foreground process for signal forwarding.
pid_t current_fg_pid = 2;
// Global job list and job counter (for background processes)
Vec job_list;           // initialize in main; holds job pointers
jid_t next_job_id = 1;  // global job id counter

// Signal handler for (Ctrl-C)
void shell_sigint_handler(int sig) {
  // If there's a foreground process, forward SIGINT (terminate) to it, as long
  // as it's not the shell current_fg_pid will also never be 1 (INIT)
  if (current_fg_pid != 2) {
    s_kill(current_fg_pid, 2);  // P_SIGTERM
  }
  // Write a new line to prompt
  write(STDERR_FILENO, "\n", 1);
  write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
}

// Set up terminal signal handlers in the shell (only for interactive mode).
void setup_terminal_signal_handlers(void) {
  struct sigaction sa_int = {0};
  sa_int.sa_handler = shell_sigint_handler;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa_int, NULL);
}

// This function will be used by vec_new as the destructor
// for each job pointer.
void free_job_ptr(void* ptr) {
  job* job_ptr = (job*)ptr;
  free(job_ptr->pids);
  free(job_ptr->cmd);
  free(job_ptr);
}

/**
 * @brief Helper function to execute a parsed command from the shell.
 * In particular, it spawns a child process to execute the command if
 * the built-in should run as a separate process. Otherwise, it just
 * calls the subroutine directly.
 *
 * @param cmd the parsed command to execute, assumed non-null
 * @return the created child id on successful spawn, 0 on successful
 *         subroutine call, -1 on error (given cmd is not a shell built-in)
 */
pid_t execute_command(struct parsed_command* cmd) {
  // TODO --> Vedansh said to use open for now?
  int input_fd = open(cmd->stdin_file,
                      F_READ);  // TODO --> error check these once implemented
  int output_fd;
  if (cmd->is_file_append) {
    output_fd = open(cmd->stdout_file, F_APPEND);
  } else {
    output_fd = open(cmd->stdout_file, F_WRITE);
  }

  // check for independently scheduled processes
  if (strcmp(cmd->commands[0][0], "cat") == 0) {
    // TODO --> make sure these files from parser are okay!
    // I'm assuming yes b/c we implement s_spawn but just check
    return s_spawn(cat, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "sleep") == 0) {
    return s_spawn(b_sleep, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "busy") == 0) {
    return s_spawn(busy, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "echo") == 0) {
    return s_spawn(echo, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "ls") == 0) {
    return s_spawn(ls, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "touch") == 0) {
    return s_spawn(touch, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "mv") == 0) {
    return s_spawn(mv, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "cp") == 0) {
    return s_spawn(cp, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "rm") == 0) {
    return s_spawn(rm, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "chmod") == 0) {
    return s_spawn(chmod, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "ps") == 0) {
    return s_spawn(ps, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "kill") == 0) {
    return s_spawn(b_kill, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "zombify") == 0) {
    return s_spawn(zombify, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "orphanify") == 0) {
    return s_spawn(orphanify, cmd->commands[0], input_fd, output_fd);
  }

  // check for sub-routines (nice, nice_pid, man, bg, fg, jobs, logout)
  if (strcmp(cmd->commands[0][0], "nice") == 0) {
    b_nice(cmd->commands[0]);
  } else if (strcmp(cmd->commands[0][0], "nice_pid") == 0) {
    nice_pid(cmd->commands[0]);
  } else if (strcmp(cmd->commands[0][0], "man") == 0) {
    man(cmd->commands[0]);
  } else if (strcmp(cmd->commands[0][0], "bg") == 0) {
    bg(cmd->commands[0]);
  } else if (strcmp(cmd->commands[0][0], "fg") == 0) {
    fg(cmd->commands[0]);
  } else if (strcmp(cmd->commands[0][0], "jobs") == 0) {
    jobs(cmd->commands[0]);
  } else if (strcmp(cmd->commands[0][0], "logout") == 0) {
    logout(cmd->commands[0]);
  } else {
    // TODO --> handle error via some valid print
    return -1;
  }

  return 0;  // only reached for subroutines
}

void* shell_main(void*) {
  // TODO --> determine if we need more here
  // add in the jobs, bg, fg stuff as needed
  // cross-check against penn-shell.c file once we
  // get answers on this

  job_list = vec_new(0, free_job_ptr);

  setup_terminal_signal_handlers();

  while (true) {
    int status;
    pid_t child_pid;
    while ((child_pid = s_waitpid(-1, &status, true)) > 0) {
      // Child process has completed, no need to do anything special
      // The s_waitpid function already handles cleanup
    }

    // prompt
    // TODO --> see if "write" instead of s_write is okay here
    /*if (s_write(STDOUT_FILENO, PROMPT, strlen(PROMPT)) < 0) {
        // TODO: remember to set PERRNO
        u_perror("prompt write error");
        break;
    }*/
    if (write(STDOUT_FILENO, PROMPT, strlen(PROMPT)) < 0) {
      perror("prompt write error");
      break;
    }

    // parse user input
    char buffer[MAX_BUFFER_SIZE];
    // TODO --> see if "write" instead of s_write is okay here
    // ssize_t user_input = s_read(STDIN_FILENO, MAX_BUFFER_SIZE, buffer);
    ssize_t user_input = read(STDIN_FILENO, buffer, MAX_BUFFER_SIZE);
    if (user_input < 0) {
      u_perror("shell read error");
      break;
    } else if (user_input == 0) {  // EOF case
      shutdown_pennos();
      break;
    }

    buffer[user_input] = '\0';
    if (buffer[user_input - 1] == '\n') {
      buffer[user_input - 1] = '\0';
    }

    struct parsed_command* cmd = NULL;
    int cmd_parse_res = parse_command(buffer, &cmd);
    if (cmd_parse_res != 0 || cmd == NULL) {
      // TODO -> handle error via some valid print
      continue;
    }

    // handle the command
    if (cmd->num_commands == 0) {
      free(cmd);
      continue;
    }

    // TODO --> handle builtins/bg as needed (if needed)

    child_pid = execute_command(cmd);
    if (child_pid < 0) {
      // TODO --> handle error via some valid print
      free(cmd);
      continue;
    }

    // If background, add the process to the job list.
    if (cmd->is_background) {
      // Create a new job entry.
      job* new_job = malloc(sizeof(job));
      if (new_job == NULL) {
        u_perror("malloc new_job");
        free(cmd);
        continue;
      }
      new_job->id = next_job_id++;
      new_job->pgid = child_pid;  // For single commands, child's pid = pgid.
      new_job->num_pids = 1;
      new_job->pids = malloc(sizeof(pid_t));
      if (new_job->pids == NULL) {
        u_perror("malloc job->pids");
        free(new_job);
        free(cmd);
        continue;
      }
      new_job->pids[0] = child_pid;
      new_job->state = RUNNING;
      new_job->cmd = cmd;  // Retain command info; do not free here.
      new_job->finished_count = 0;
      vec_push_back(&job_list, new_job);

      // Print job control information in the format: "[job_id] child_pid"
      char msg[128];
      snprintf(msg, sizeof(msg), "[%lu] %d\n", (unsigned long)new_job->id,
               child_pid);
      write(STDOUT_FILENO, msg, strlen(msg));
    } else {
      // Foreground execution.
      current_fg_pid = child_pid;
      int status;
      s_waitpid(child_pid, &status, false);
      current_fg_pid = 2;
      // Free cmd memory for foreground commands.
      free(cmd);
    }
  }
  vec_destroy(&job_list);
  s_exit();
  return 0;
}
