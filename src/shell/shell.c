/* CS5480 PennOS Group 61
 * Authors: Krystof Purtell and Richard Zhang
 * Purpose: Implements a shell that can run built-in commands and scripts.
 */

#include <fcntl.h>
#include <string.h>
#include "../fs/fat_routines.h"
#include "../fs/fs_helpers.h"
#include "../fs/fs_syscalls.h"
#include "../kernel/kern_sys_calls.h"
#include "../kernel/scheduler.h"
#include "../kernel/stress.h"
#include "../lib/Vec.h"
#include "Job.h"
#include "builtins.h"
#include "lib/pennos-errno.h"
#include "parser.h"
#include "shell_built_ins.h"
#include "signal.h"
#include "stdio.h"
#include "stdlib.h"

#ifndef PROMPT
#define PROMPT "$ "
#endif

#define MAX_BUFFER_SIZE 4096
#define MAX_LINE_BUFFER_SIZE 128

// Global variable to track foreground process for signal forwarding.
extern pid_t current_fg_pid;
// Global job list and job counter (for background processes)
Vec job_list;           // initialize in main; holds job pointers
jid_t next_job_id = 1;  // global job id counter

// global variables for script execution, to prevent major arguments refactoring
int script_fd = -1;
int input_fd_script = -1;
int output_fd_script = -1;
int is_append = 0;

///////////////////////////////////////////////////////////////////////////////////////////
//                              Signals Setup //
///////////////////////////////////////////////////////////////////////////////////////////

// Signal handler for (Ctrl-C)
void shell_sigint_handler(int sig) {
  // If there's a foreground process, forward SIGINT (terminate) to it, as long
  // as it's not the shell current_fg_pid will also never be 1 (INIT)
  if (current_fg_pid != 2) {
    s_kill(current_fg_pid, 2);  // P_SIGTERM
  }

  if (s_write(STDOUT_FILENO, "\n", 1) == -1) {
    u_perror("s_write error");
  }
}

// Signal handler for (Ctrl-Z)
void shell_sigstp_handler(int sig) {
  // If there's a foreground process, forward SIGTSTP (stop) to it
  if (current_fg_pid != 2) {
    s_kill(current_fg_pid, 0);  // P_SIGSTOP
  }

  if (s_write(STDOUT_FILENO, "\n", 1) == -1) {
    u_perror("s_write error");
  }
}

// Set up terminal signal handlers in the shell (only for interactive mode).
void setup_terminal_signal_handlers(void) {
  struct sigaction sa_int = {0};
  sa_int.sa_handler = shell_sigint_handler;
  sigemptyset(&sa_int.sa_mask);
  sa_int.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa_int, NULL) == -1) {
    P_ERRNO = P_ESIGNAL;
    u_perror("sigaction");
    exit(EXIT_FAILURE);
  }

  struct sigaction sa_stp = {0};
  sa_stp.sa_handler = shell_sigstp_handler;
  sigemptyset(&sa_stp.sa_mask);
  sa_stp.sa_flags = SA_RESTART;
  if (sigaction(SIGTSTP, &sa_stp, NULL) == -1) {
    P_ERRNO = P_ESIGNAL;
    u_perror("sigaction");
    exit(EXIT_FAILURE);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////
//                              Job Management //
///////////////////////////////////////////////////////////////////////////////////////////

// This function will be used by vec_new as the destructor
// for each job pointer.
void free_job_ptr(void* ptr) {
  job* job_ptr = (job*)ptr;
  free(job_ptr->pids);
  free(job_ptr);
}

//////////////////////////////////////////////////////////////////////////////////////////
//                     Command and Script Execution Functions //
//////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Helper function that fills a buffer with characters read from a given
 *        file descriptor until the buffer is full (rare and impractical case),
 *        a newline is encountered, or EOF is reached.
 *
 * @param fd      the file descriptor to read from, assumed to be open
 * @param buffer  the buffer to fill with characters
 */
void fill_buffer_until_full_or_newline(int fd, char* buffer) {
  int i = 0;
  char currChar;
  while (i < MAX_LINE_BUFFER_SIZE - 1) {
    int bytes_read = s_read(fd, &currChar, 1);
    if (bytes_read <= 0 || currChar == '\n') {  // EOF or newline cases
      break;
    }
    buffer[i] = currChar;
    i++;
  }
  buffer[i] = '\0';  // Null-terminate the string, replaces \n
}

/**
 * @brief Helper function that will execute a given command so long as it's
 *        one of the built-ins. Notably, its output and input are determined
 *        by the spawning script process.
 *
 * @param cmd the parsed command to try executing
 * @return    the pid of the process if one was spawned, 0 if a routine was run
 *            or -1 if not matches found
 */
pid_t u_execute_command(struct parsed_command* cmd) {
  // check for independently scheduled processes
  if (strcmp(cmd->commands[0][0], "cat") == 0) {
    return s_spawn(u_cat, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "sleep") == 0) {
    return s_spawn(u_sleep, cmd->commands[0], input_fd_script,
                   output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "busy") == 0) {
    return s_spawn(u_busy, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "echo") == 0) {
    return s_spawn(u_echo, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "ls") == 0) {
    return s_spawn(u_ls, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "touch") == 0) {
    return s_spawn(u_touch, cmd->commands[0], input_fd_script,
                   output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "mv") == 0) {
    return s_spawn(u_mv, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "cp") == 0) {
    return s_spawn(u_cp, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "rm") == 0) {
    return s_spawn(u_rm, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "chmod") == 0) {
    return s_spawn(u_chmod, cmd->commands[0], input_fd_script,
                   output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "ps") == 0) {
    return s_spawn(u_ps, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "kill") == 0) {
    return s_spawn(u_kill, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "zombify") == 0) {
    return s_spawn(u_zombify, cmd->commands[0], input_fd_script,
                   output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "orphanify") == 0) {
    return s_spawn(u_orphanify, cmd->commands[0], input_fd_script,
                   output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "hang") == 0) {
    return s_spawn(hang, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "nohang") == 0) {
    return s_spawn(nohang, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "recur") == 0) {
    return s_spawn(recur, cmd->commands[0], input_fd_script, output_fd_script);
  } else if (strcmp(cmd->commands[0][0], "crash") == 0) {
    return s_spawn(crash, cmd->commands[0], input_fd_script, output_fd_script);
  }

  // check for sub-routines
  if (strcmp(cmd->commands[0][0], "nice") == 0) {
    u_nice(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "nice_pid") == 0) {
    u_nice_pid(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "man") == 0) {
    u_man(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "bg") == 0) {
    u_bg(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "fg") == 0) {
    u_fg(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "jobs") == 0) {
    u_jobs(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "logout") == 0) {
    u_logout(cmd->commands[0]);
    return 0;
  } else {
    return -1;  // no matches, no scripts now
  }

  return 0;
}

/**
 * @brief Helper function that reads a script file line by line, parses each
 *        line as a command, and executes it.
 *
 * @param arg standard {function name, NULL} args
 * @return    NULL
 */
void* u_read_and_execute_script(void* arg) {
  // read the script line by line, parse each line, and execute the command
  while (true) {
    char buffer[MAX_LINE_BUFFER_SIZE];
    fill_buffer_until_full_or_newline(script_fd, buffer);
    if (buffer[0] == '\0') {
      break;  // EOF case
    }

    // parse the command
    struct parsed_command* cmd = NULL;
    int parse_result = parse_command(buffer, &cmd);
    if (parse_result != 0 || cmd == NULL) {
      P_ERRNO = P_EPARSE;
      u_perror("parse_command");
      free(cmd);
    }

    // execute the command
    pid_t child_pid = u_execute_command(cmd);
    if (child_pid > 0) {  // if process was spawned, wait for it to finish
      int status;
      s_waitpid(child_pid, &status, false);
    } else if (child_pid < 0) {  // nothing spawning so safe to free cmd
      free(cmd);
    }
  }

  s_exit();  // exit the script
  return NULL;
}

/**
 * @brief Helper function to execute a parsed command from the shell.
 * In particular, it spawns a child process to execute the command if
 * the built-in should run as a separate process. Otherwise, it just
 * calls the subroutine directly.
 *
 * @param cmd the parsed command to execute, assumed non-null
 * @return the created child id on successful spawn, 0 on successful
 *         subroutine call, -1 when nothing was called
 */
pid_t execute_command(struct parsed_command* cmd) {
  // setup fds
  int input_fd = STDIN_FILENO;  // standard fds
  int output_fd = STDOUT_FILENO;

  if (cmd->stdin_file != NULL) {
    input_fd = s_open(cmd->stdin_file, F_READ);
    if (input_fd < 0) {
      input_fd = STDIN_FILENO;  // reset to default
    }
  }

  if (cmd->is_file_append) {
    output_fd = s_open(cmd->stdout_file, F_APPEND);
    is_append = 1;
  } else {
    output_fd = s_open(cmd->stdout_file, F_WRITE);
    is_append = 0;
  }
  if (output_fd < 0) {
    output_fd = STDOUT_FILENO;  // reset to default
  }

  // check for independently scheduled processes
  if (strcmp(cmd->commands[0][0], "cat") == 0) {
    return s_spawn(u_cat, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "sleep") == 0) {
    return s_spawn(u_sleep, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "busy") == 0) {
    return s_spawn(u_busy, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "echo") == 0) {
    return s_spawn(u_echo, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "ls") == 0) {
    return s_spawn(u_ls, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "touch") == 0) {
    return s_spawn(u_touch, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "mv") == 0) {
    return s_spawn(u_mv, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "cp") == 0) {
    return s_spawn(u_cp, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "rm") == 0) {
    return s_spawn(u_rm, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "chmod") == 0) {
    return s_spawn(u_chmod, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "ps") == 0) {
    return s_spawn(u_ps, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "kill") == 0) {
    return s_spawn(u_kill, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "zombify") == 0) {
    return s_spawn(u_zombify, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "orphanify") == 0) {
    return s_spawn(u_orphanify, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "hang") == 0) {
    return s_spawn(hang, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "nohang") == 0) {
    return s_spawn(nohang, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "recur") == 0) {
    return s_spawn(recur, cmd->commands[0], input_fd, output_fd);
  } else if (strcmp(cmd->commands[0][0], "crash") == 0) {
    return s_spawn(crash, cmd->commands[0], input_fd, output_fd);
  }

  // check for sub-routines
  if (strcmp(cmd->commands[0][0], "nice") == 0) {
    u_nice(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "nice_pid") == 0) {
    u_nice_pid(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "man") == 0) {
    u_man(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "bg") == 0) {
    u_bg(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "fg") == 0) {
    u_fg(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "jobs") == 0) {
    u_jobs(cmd->commands[0]);
    return 0;
  } else if (strcmp(cmd->commands[0][0], "logout") == 0) {
    u_logout(cmd->commands[0]);
    return 0;
  }

  // otherwise, try to run command as a script
  int script_fd_open = s_open(cmd->commands[0][0], F_READ);
  if (script_fd_open < 0) {  // if not a file, just move on
    return -1;
  }
  if (has_executable_permission(script_fd_open) != 1) {
    if (s_close(script_fd_open) == -1) {
      u_perror("s_close error i.e. not a valid fd");
    }
    return -1;
  } else {
    script_fd = script_fd_open;  // update global
    input_fd_script = input_fd;
    output_fd_script = output_fd;

    char* script_argv[] = {cmd->commands[0][0], NULL};
    pid_t wait_on =
        s_spawn(u_read_and_execute_script, script_argv, input_fd, output_fd);
    int status;
    s_waitpid(wait_on, &status, false);  // wait for script to finish
    script_fd = -1;                      // reset global
    input_fd_script = STDIN_FILENO;
    output_fd_script = STDOUT_FILENO;
    if (s_close(script_fd_open) == -1) {
      u_perror("s_close error i.e. not a valid fd");
    }
    return 0;
  }

  return -1;  // no matches case
}

//////////////////////////////////////////////////////////////////////////////////
//                        Shell main function //
//////////////////////////////////////////////////////////////////////////////////

void* shell(void*) {
  job_list = vec_new(0, free_job_ptr);

  setup_terminal_signal_handlers();

  while (true) {
    // poll background jobs
    int status;
    pid_t child_pid;
    while ((child_pid = s_waitpid(-1, &status, true)) > 0) {
      // Find which job child_pid belongs to
      for (size_t i = 0; i < vec_len(&job_list); i++) {
        job* job = vec_get(&job_list, i);
        bool in_this_job = false;
        for (size_t j = 0; j < job->num_pids; j++) {
          if (job->pids[j] == child_pid) {
            in_this_job = true;
            break;
          }
        }

        if (!in_this_job) {
          continue;
        }

        // If the process ended normally or via signal
        if (P_WIFEXITED(status) || P_WIFSIGNALED(status)) {
          job->finished_count++;
          if (job->finished_count == job->num_pids) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Finished: ");
            s_write(STDOUT_FILENO, buf, strlen(buf));
            for (size_t cmdIdx = 0; cmdIdx < job->cmd->num_commands; cmdIdx++) {
              char** argv = job->cmd->commands[cmdIdx];
              int argIdx = 0;
              while (argv[argIdx] != NULL) {
                snprintf(buf, sizeof(buf), "%s ", argv[argIdx]);
                s_write(STDOUT_FILENO, buf, strlen(buf));
                argIdx++;
              }
            }
            snprintf(buf, sizeof(buf), "\n");
            s_write(STDOUT_FILENO, buf, strlen(buf));
            vec_erase(&job_list, i);
          }
        } else if (P_WIFSTOPPED(status) && job->state == RUNNING) {
          job->state = STOPPED;
          char buf[128];
          snprintf(buf, sizeof(buf), "Stopped: ");
          s_write(STDOUT_FILENO, buf, strlen(buf));
          for (size_t cmdIdx = 0; cmdIdx < job->cmd->num_commands; cmdIdx++) {
            char** argv = job->cmd->commands[cmdIdx];
            int argIdx = 0;
            while (argv[argIdx] != NULL) {
              snprintf(buf, sizeof(buf), "%s ", argv[argIdx]);
              s_write(STDOUT_FILENO, buf, strlen(buf));
              argIdx++;
            }
          }
          snprintf(buf, sizeof(buf), "\n");
          s_write(STDOUT_FILENO, buf, strlen(buf));
        }
        break;  // break from for-loop over job_list
      }
    }

    // prompt
    if (s_write(STDOUT_FILENO, PROMPT, strlen(PROMPT)) < 0) {
      u_perror("prompt s_write error");
      break;
    }

    // parse user input
    char buffer[MAX_BUFFER_SIZE];
    ssize_t user_input = s_read(STDIN_FILENO, buffer, MAX_BUFFER_SIZE);
    if (user_input < 0) {
      u_perror("shell read error");
      break;
    } else if (user_input == 0) {  // EOF case
      s_shutdown_pennos();
      break;
    }

    buffer[user_input] = '\0';
    if (buffer[user_input - 1] == '\n') {
      buffer[user_input - 1] = '\0';
    }

    struct parsed_command* cmd = NULL;
    int cmd_parse_res = parse_command(buffer, &cmd);
    if (cmd_parse_res != 0 || cmd == NULL) {
      P_ERRNO = P_EPARSE;
      u_perror("parse_command");
      continue;
    }

    // handle the command
    if (cmd->num_commands == 0) {
      free(cmd);
      continue;
    }

    child_pid = execute_command(cmd);
    if (child_pid < 0) {
      free(cmd);
      continue;
    } else if (child_pid == 0) {
      free(cmd);
      continue;
    }

    // If background, add the process to the job list.
    if (cmd->is_background) {
      // Create a new job entry.
      job* new_job = malloc(sizeof(job));
      if (new_job == NULL) {
        perror("Error: mallocing new_job failed");
        free(cmd);
        continue;
      }
      new_job->id = next_job_id++;
      new_job->pgid = child_pid;  // For single commands, child's pid = pgid.
      new_job->num_pids = 1;
      new_job->pids = malloc(sizeof(pid_t));
      if (new_job->pids == NULL) {
        perror("Error: mallocing new_job->pids failed");
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
      if (s_write(STDOUT_FILENO, msg, strlen(msg)) == -1) {
        u_perror("s_write error");
      }
    } else {
      // Foreground execution.
      current_fg_pid = child_pid;
      int status;
      s_waitpid(child_pid, &status, false);

      if (P_WIFSTOPPED(status)) {
        // Create a new job entry (this time for a stopped process)
        job* new_job = malloc(sizeof(job));
        if (new_job == NULL) {
          perror("Error: mallocing new_job failed");
          free(cmd);
          continue;
        }
        new_job->id = next_job_id++;
        new_job->pgid = child_pid;  // For single commands, child's pid = pgid.
        new_job->num_pids = 1;
        new_job->pids = malloc(sizeof(pid_t));
        if (new_job->pids == NULL) {
          perror("Error: mallocing new_job->pids failed");
          free(new_job);
          free(cmd);
          continue;
        }
        new_job->pids[0] = child_pid;
        new_job->state = STOPPED;
        new_job->cmd = cmd;  // Retain command info; do not free here.
        new_job->finished_count = 0;
        vec_push_back(&job_list, new_job);

        // Print stopped job
        char buf[128];
        snprintf(buf, sizeof(buf), "Stopped: ");
        s_write(STDOUT_FILENO, buf, strlen(buf));
        for (size_t cmdIdx = 0; cmdIdx < new_job->cmd->num_commands; cmdIdx++) {
          char** argv = new_job->cmd->commands[cmdIdx];
          int argIdx = 0;
          while (argv[argIdx] != NULL) {
            snprintf(buf, sizeof(buf), "%s ", argv[argIdx]);
            s_write(STDOUT_FILENO, buf, strlen(buf));
            argIdx++;
          }
        }
        snprintf(buf, sizeof(buf), "\n");
        s_write(STDOUT_FILENO, buf, strlen(buf));
      }

      current_fg_pid = 2;

      // Free cmd memory for foreground commands.
      // free(cmd); // TODO --> check if this is already freed, it may be
    }
  }

  vec_destroy(&job_list);
  s_exit();
  return 0;
}
