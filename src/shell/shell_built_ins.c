#include "shell_built_ins.h"

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include "../kernel/kern_pcb.h"  // TODO --> this is a little dangerous,
#include "../lib/Vec.h"          // make sure not to use k funcs
#include <string.h>
#include "../fs/fs_syscalls.h"
#include "../kernel/kern_sys_calls.h"
#include "../lib/spthread.h"
#include "../kernel/scheduler.h" // TODO --> make sure this is allowed, otw make wrapper
#include "../fs/fat_routines.h"

#include <stdio.h>   // I think this is okay? Using snprintf
#include <stdlib.h>  // For strtol
#include <unistd.h>  // probably delete once done
#include <errno.h>   // For errno for strtol

extern Vec current_pcbs;

////////////////////////////////////////////////////////////////////////////////
//        The following shell built-in routines should run as                 //
//        independently scheduled PennOS processes.                           //
////////////////////////////////////////////////////////////////////////////////

void* u_cat(void *arg) {
  cat(arg);
  s_exit();
  return NULL;
}

void* u_sleep(void* arg) {
  char* endptr;
  errno = 0;
  if (((char**)arg)[1] == NULL) { // no arg case
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
  // TODO --> implement echo
  return NULL;
}

void* u_ls(void *arg) {
  ls(arg);
  s_exit();
  return NULL;
}

void* u_touch(void *arg) {
  touch(arg);
  s_exit();
  return NULL;
}

void* u_mv(void *arg) {
  mv(arg);
  s_exit();
  return NULL;
}

void* u_cp(void *arg) {
  cp(arg);
  s_exit();
  return NULL;
}

void* u_rm(void *arg) {
  rm(arg);
  s_exit();
  return NULL;
}

void* u_chmod(void* arg) {
  // TODO --> implement chmod
  return NULL;
}

void* u_ps(void* arg) {
  char pid_top[] = "PID\tPPID\tPRI\tSTAT\tCMD\n";
  s_write(STDOUT_FILENO, pid_top, strlen(pid_top)); 
  for (int i = 0; i < vec_len(&current_pcbs); i++) {
    pcb_t* curr_pcb = (pcb_t*)vec_get(&current_pcbs, i);
    char buffer[100];
    snprintf(buffer, sizeof(buffer), "%d\t%d\t%d\t%c\t%s\n", curr_pcb->pid,
             curr_pcb->par_pid, curr_pcb->priority, curr_pcb->process_state,
             curr_pcb->cmd_str);                   // TODO --> is this allowed?
    s_write(STDOUT_FILENO, buffer, strlen(buffer)); 
  }
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
      snprintf(err_buf, sizeof(err_buf), "Invalid signal specification: %s\n",
               argv[start_index]);
      s_write(STDERR_FILENO, err_buf, strlen(err_buf));
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
  
  return NULL; // no matches case
}

void* u_nice(void* arg) {
  char* endptr;
  errno = 0;
  int new_priority = (int)strtol(((char**)arg)[1], &endptr, 10);
  if (*endptr != '\0' || errno != 0 || new_priority > 2 || new_priority < 0) {  // error catch
    return NULL;
  }

  char* command = ((char**)arg)[2];
  void* (*ufunc)(void*) = get_associated_ufunc(command);
  if (ufunc == NULL) {
    return NULL;  // no matches, don't spawn
  }

  pid_t new_proc_pid = s_spawn(ufunc, &((char**)arg)[2], 0, 1);  // TODO --> check these fds

  if (new_proc_pid != -1) { // non-error case
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
  "cat f1 f2 ...        : concatenates provided files (if none, reads from std in), and writes to std out\n"
  "sleep n               : sleeps for n seconds\n"
  "busy                  : busy waits indefinitely\n"
  "echo str              : echoes back the input string str\n"
  "ls                    : lists all files in the working directory\n"
  "touch f1 f2 ...       : for each file, creates empty file if it doesn't exist yet, otherwise updates its timestamp\n"
  "mv f1 f2              : renames f1 to f2 (overwrites f2 if it exists)\n"
  "cp f1 f2              : copies f1 to f2 (overwrites f2 if it exists)\n"
  "rm f1 f2 ...          : removes the input list of files\n"
  "chmod +_ f1           : changes f1 permissions to +_ specifications (+x, +rw, etc)\n"
  "ps                    : lists all processes on PennOS, displaying PID, PPID, priority, status, and command name\n"
  "kill (-__) pid1 pid 2 : sends specified signal (term default) to list of processes\n"
  "nice n command        : spawns a new process for command and sets its priority to n\n"
  "nice_pid n pid        : adjusts the priority level of an existing process to n\n"
  "man                   : lists all available commands in PennOS\n"
  "bg                    : resumes most recently stopped process in background or the one specified by job_id\n"
  "fg                    : brings most recently stopped or background job to foreground or the one specifed by job_id\n"
  "jobs                  : lists all jobs\n"
  "logout                : exits the shell and shuts down PennOS\n"
  "zombify               : creates a child process that becomes a zombie\n"
  "orphanify             : creates a child process that becomes an orphan\n";
  
  s_write(STDOUT_FILENO, man_string, strlen(man_string)); 
  return NULL;
}

void* u_bg(void* arg) {
  // TODO --> implement bg
  return NULL;
}

void* u_fg(void* arg) {
  // TODO --> implement fg
  return NULL;
}

void* u_jobs(void* arg) {
  // TODO --> implement jobs
  return NULL;
}

void* u_logout(void* arg) {
  shutdown_pennos();
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
  while (1);
  return NULL;
}

/**
 * @brief Helper for orphanify.
 */
void* orphan_child(void* arg) {
  while (1);
  s_exit();
}

void* u_orphanify(void* arg) {
  char* orphan_child_argv[] = {"orphan_child", NULL};
  s_spawn(orphan_child, orphan_child_argv, STDIN_FILENO, STDOUT_FILENO);
  s_exit();
  return NULL;
}