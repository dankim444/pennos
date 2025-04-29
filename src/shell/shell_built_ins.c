#include "shell_built_ins.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include "../fs/fat_routines.h"
#include "../fs/fs_syscalls.h"
#include "../kernel/kern_pcb.h"  // TODO --> this is a little dangerous,
#include "../kernel/kern_sys_calls.h"
#include "../kernel/scheduler.h"  // TODO --> make sure this is allowed, otw make wrapper
#include "../kernel/signal.h"
#include "../lib/Vec.h"  // make sure not to use k funcs
#include "../lib/spthread.h"

#include <errno.h>   // For errno for strtol
#include <stdio.h>   // I think this is okay? Using snprintf
#include <stdlib.h>  // For strtol
#include <unistd.h>  // probably delete once done

#include "Job.h"

// needed for job control
extern Vec job_list;
extern pid_t current_fg_pid;

////////////////////////////////////////////////////////////////////////////////
//        The following shell built-in routines should run as                 //
//        independently scheduled PennOS processes.                           //
////////////////////////////////////////////////////////////////////////////////

void* u_cat(void* arg) {
  cat(arg);
  s_exit();
  return NULL;
}

void* u_sleep(void* arg) {
  char* endptr;
  errno = 0;
  if (((char**)arg)[1] == NULL) {  // no arg case
    s_exit();
    return NULL;
  }
  int sleep_secs = (int)strtol(((char**)arg)[1], &endptr, 10);
  if (*endptr != '\0' || errno != 0 || sleep_secs <= 0) {
    s_exit();
    return NULL;
  }

  int sleep_ticks = sleep_secs * 10;
  s_sleep(sleep_ticks);
  s_exit();
  return NULL;
}

void* u_busy(void* arg) {
  while (1)
    ;
  s_exit();
  return NULL;
}

void* u_echo(void* arg) {
  s_echo(arg);
  s_exit();
  return NULL;
}

void* u_ls(void* arg) {
  ls(arg);
  s_exit();
  return NULL;
}

void* u_chmod(void* arg) {
  chmod(arg);
  s_exit();
  return NULL;
}

void* u_touch(void* arg) {
  touch(arg);
  s_exit();
  return NULL;
}

void* u_mv(void* arg) {
  mv(arg);
  s_exit();
  return NULL;
}

void* u_cp(void* arg) {
  cp(arg);
  s_exit();
  return NULL;
}

void* u_rm(void* arg) {
  rm(arg);
  s_exit();
  return NULL;
}

void* u_ps(void* arg) {
  s_ps(arg);
  s_exit();
  return NULL;
}

void* u_kill(void* arg) {
  char** argv = (char**)arg;
  int sig = 2;          // Default signal: term (2)
  int start_index = 1;  // Start after the "kill" command word.
  char err_buf[128];

  // Check if the first argument specifies a signal
  if (argv[start_index] && argv[start_index][0] == '-') {
    if (strcmp(argv[start_index], "-term") == 0) {
      sig = 2;
    } else if (strcmp(argv[start_index], "-stop") == 0) {
      sig = 0;
    } else if (strcmp(argv[start_index], "-cont") == 0) {
      sig = 1;
    } else {
      // Construct error message
      s_exit();
      return NULL;
    }
    start_index++;
  }

  // Process each PID argument using strtol
  for (int i = start_index; argv[i] != NULL; i++) {
    char* endptr;
    long pid_long = strtol(argv[i], &endptr, 10);
    if (*endptr != '\0' || pid_long <= 0) {
      snprintf(err_buf, 128, "Invalid PID: %s\n", argv[i]);
      s_write(STDERR_FILENO, err_buf, strlen(err_buf));
      continue;
    }
    pid_t pid = (pid_t)pid_long;
    if (s_kill(pid, sig) < 0) {
      snprintf(err_buf, 128, "b_kill error on PID %d\n", pid);
      s_write(STDERR_FILENO, err_buf, strlen(err_buf));
    }
  }
  s_exit();
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
//     The following shell built-ins should be run as the shell; that is,     //
//     they should each execute as a shell sub-routine directly rather        //
//     than as an independent process.                                        //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Helper function to get the associated "u-version" of a function
 *        given its standalone version. As a conrete example, if we pass
 *        in "cat", we will output "u_cat"
 *
 * @param func A string of the function name to get the associated ufunc for
 * @return     A ptr to the associated u-version function or NULL if no
 *             matches are found
 */
void* (*get_associated_ufunc(char* func))(void*) {
  if (strcmp(func, "cat") == 0) {
    return u_cat;
  } else if (strcmp(func, "sleep") == 0) {
    return u_sleep;
  } else if (strcmp(func, "busy") == 0) {
    return u_busy;
  } else if (strcmp(func, "echo") == 0) {
    return u_echo;
  } else if (strcmp(func, "ls") == 0) {
    return u_ls;
  } else if (strcmp(func, "touch") == 0) {
    return u_touch;
  } else if (strcmp(func, "mv") == 0) {
    return u_mv;
  } else if (strcmp(func, "cp") == 0) {
    return u_cp;
  } else if (strcmp(func, "rm") == 0) {
    return u_rm;
  } else if (strcmp(func, "chmod") == 0) {
    return u_chmod;
  } else if (strcmp(func, "ps") == 0) {
    return u_ps;
  } else if (strcmp(func, "kill") == 0) {
    return u_kill;
  }

  return NULL;  // no matches case
}

void* u_nice(void* arg) {
  char* endptr;
  errno = 0;
  int new_priority = (int)strtol(((char**)arg)[1], &endptr, 10);
  if (*endptr != '\0' || errno != 0 || new_priority > 2 ||
      new_priority < 0) {  // error catch
    return NULL;
  }

  char* command = ((char**)arg)[2];
  void* (*ufunc)(void*) = get_associated_ufunc(command);
  if (ufunc == NULL) {
    return NULL;  // no matches, don't spawn
  }

  pid_t new_proc_pid = s_spawn(ufunc, &((char**)arg)[2], 0,
                               1);  // TODO --> check these fds THESE ARE WRONG
                                    // FIX, should allowed redirection

  if (new_proc_pid != -1) {  // non-error case
    s_nice(new_proc_pid, new_priority);
  }

  return NULL;
}

void* u_nice_pid(void* arg) {
  char* endptr;
  errno = 0;
  int new_priority = (int)strtol(((char**)arg)[1], &endptr, 10);
  if (*endptr != '\0' || errno != 0) {  // error catch
    return NULL;
  }
  pid_t pid = (pid_t)strtol(((char**)arg)[2], &endptr, 10);
  if (*endptr != '\0' || errno != 0) {
    return NULL;
  }
  s_nice(pid, new_priority);
  return NULL;
}

void* u_man(void* arg) {
  const char* man_string =
      "cat f1 f2 ...        : concatenates provided files (if none, reads from "
      "std in), and writes to std out\n"
      "sleep n               : sleeps for n seconds\n"
      "busy                  : busy waits indefinitely\n"
      "echo str              : echoes back the input string str\n"
      "ls                    : lists all files in the working directory\n"
      "touch f1 f2 ...       : for each file, creates empty file if it doesn't "
      "exist yet, otherwise updates its timestamp\n"
      "mv f1 f2              : renames f1 to f2 (overwrites f2 if it exists)\n"
      "cp f1 f2              : copies f1 to f2 (overwrites f2 if it exists)\n"
      "rm f1 f2 ...          : removes the input list of files\n"
      "chmod +_ f1           : changes f1 permissions to +_ specifications "
      "(+x, +rw, etc)\n"
      "ps                    : lists all processes on PennOS, displaying PID, "
      "PPID, priority, status, and command name\n"
      "kill (-__) pid1 pid 2 : sends specified signal (term default) to list "
      "of processes\n"
      "nice n command        : spawns a new process for command and sets its "
      "priority to n\n"
      "nice_pid n pid        : adjusts the priority level of an existing "
      "process to n\n"
      "man                   : lists all available commands in PennOS\n"
      "bg                    : resumes most recently stopped process in "
      "background or the one specified by job_id\n"
      "fg                    : brings most recently stopped or background job "
      "to foreground or the one specifed by job_id\n"
      "jobs                  : lists all jobs\n"
      "logout                : exits the shell and shuts down PennOS\n"
      "zombify               : creates a child process that becomes a zombie\n"
      "orphanify             : creates a child process that becomes an "
      "orphan\n";

  s_write(STDOUT_FILENO, man_string, strlen(man_string));
  return NULL;
}

void print_all_job_commands(void) {
  char buf[128];
  for (size_t ji = 0; ji < vec_len(&job_list); ji++) {
    job* j = vec_get(&job_list, ji);
    char** argv = j->cmd->commands[0];
    for (int ai = 0; argv[ai] != NULL; ai++) {
      int n = snprintf(buf, sizeof(buf), "%s ", argv[ai]);
      s_write(STDOUT_FILENO, buf, n);
    }
    s_write(STDOUT_FILENO, "\n", 1);
  }
}

// helpers for job control here (carried from pennshell)
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

void* u_bg(void* arg) {
  char buf[128];
  char** argv = (char**)arg;
  const char* jobArg = argv[1];  // NULL if no ID was given
  job* job_ptr = findJobByIdOrCurrent(jobArg);
  if (!job_ptr) {
    snprintf(buf, sizeof(buf), "bg: no such job\n");
    s_write(STDERR_FILENO, buf, strlen(buf));
    return NULL;
  }
  if (job_ptr->state == STOPPED) {
    job_ptr->state = RUNNING;
    snprintf(buf, sizeof(buf), "Running: ");
    s_write(STDOUT_FILENO, buf, strlen(buf));
    for (size_t cmdIdx = 0; cmdIdx < job_ptr->cmd->num_commands; cmdIdx++) {
      char** argv = job_ptr->cmd->commands[cmdIdx];
      int argIdx = 0;
      while (argv[argIdx] != NULL) {
        snprintf(buf, sizeof(buf), "%s ", argv[argIdx]);
        s_write(STDOUT_FILENO, buf, strlen(buf));
        argIdx++;
      }
    }
    snprintf(buf, sizeof(buf), "\n");
    s_write(STDOUT_FILENO, buf, strlen(buf));
    // P_SIGCONT is 1
    s_kill(job_ptr->pids[0], P_SIGCONT);
    return NULL;
  } else if (job_ptr->state == RUNNING) {
    snprintf(buf, sizeof(buf), "bg: job [%lu] is already running\n",
             (unsigned long)job_ptr->id);
    s_write(STDOUT_FILENO, buf, strlen(buf));
    return NULL;
  } else {
    snprintf(buf, sizeof(buf), "bg: job [%lu] not stopped\n",
             (unsigned long)job_ptr->id);
    s_write(STDOUT_FILENO, buf, strlen(buf));
    return NULL;
  }
}

void* u_fg(void* arg) {
  // TODO --> implement fg
  return NULL;
}

// straight up same as pennshell just snprintf instead of fprintf
void* u_jobs(void* arg) {
  char buf[128];
  if (vec_is_empty(&job_list)) {
    return NULL;
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

    snprintf(buf, sizeof(buf), "[%lu] ", (unsigned long)job_ptr->id);
    s_write(STDOUT_FILENO, buf, strlen(buf));
    for (size_t cmdIdx = 0; cmdIdx < job_ptr->cmd->num_commands; cmdIdx++) {
      char** argv = job_ptr->cmd->commands[cmdIdx];
      int argIdx = 0;
      while (argv[argIdx] != NULL) {
        snprintf(buf, sizeof(buf), "%s ", argv[argIdx]);
        s_write(STDOUT_FILENO, buf, strlen(buf));
        argIdx++;
      }
    }
    snprintf(buf, sizeof(buf), "(%s)\n", state);
    s_write(STDOUT_FILENO, buf, strlen(buf));
  }
  return NULL;
}

void* u_logout(void* arg) {
  s_shutdown_pennos();
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
//     Zombie and orphan testing functions                                    //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Helper for zombify.
 */
void* zombie_child(void* arg) {
  s_exit();
  return NULL;
}

void* u_zombify(void* arg) {
  char* zombie_child_argv[] = {"zombie_child", NULL};
  s_spawn(zombie_child, zombie_child_argv, STDIN_FILENO, STDOUT_FILENO);
  while (1)
    ;
  return NULL;
}

/**
 * @brief Helper for orphanify.
 */
void* orphan_child(void* arg) {
  while (1)
    ;
  s_exit();
}

void* u_orphanify(void* arg) {
  char* orphan_child_argv[] = {"orphan_child", NULL};
  s_spawn(orphan_child, orphan_child_argv, STDIN_FILENO, STDOUT_FILENO);
  s_exit();
  return NULL;
}
