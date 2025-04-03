/* CS5480 Project 02 penn-shell
 * Authors: Dan Kim and Richard Zhang
 * Purpose: Implements penn-shell.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "Job.h"
#include "Vec.h"
#include "executor.h"
#include "job_controller.h"
#include "parser.h"
#include "poll.h"
#include "signals.h"

#ifndef PROMPT
#define PROMPT "penn-shell> "
#endif

// globals
bool interactive_mode;
Vec job_list;
jid_t next_job_id = 1;

// shell main loop
int main(int argc, char** argv) {
  interactive_mode = isatty(STDIN_FILENO);
  init_job_vector();

  // set up signals
  if (interactive_mode) {
    setup_signal_handlers();
  }

  // if interactive, put the shell in its own PGID and grab control of the
  // terminal
  if (interactive_mode) {
    pid_t shellPid = getpid();
    if (setpgid(shellPid, shellPid) < 0) {
      perror("setpgid(shell)");
      exit(EXIT_FAILURE);
    }
    if (tcsetpgrp(STDIN_FILENO, shellPid) < 0) {
      perror("tcsetpgrp(shell)");
      exit(EXIT_FAILURE);
    }
  }

  char* buffer = NULL;
  size_t bufferCapacity = 0;

  while (true) {
    // poll background jobs
    poll_background_jobs();

    if (interactive_mode) {
      ssize_t wrote = write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
      if (wrote == -1) {
        perror("Write error, prompt");
        break;
      }
    }

    // get input from buffer, exit if ctrl-D or EOF
    ssize_t len = getline(&buffer, &bufferCapacity, stdin);
    if (len == -1) {
      if (feof(stdin)) {
        break;  // exit
      }
      if (errno == EINTR) {
        // got interrupted by SIGINT / SIGTSTP
        clearerr(stdin);
        continue;  // re-prompt
      }
      perror("getline");
      break;
    }

    // null-terminate if needed
    if (len > 0 && buffer[len - 1] == '\n') {
      buffer[len - 1] = '\0';
    }

    // parse the buffer for the command, check for errors
    struct parsed_command* parsedResult = NULL;
    int parsedValue = parse_command(buffer, &parsedResult);
    if (parsedValue != 0) {
      fprintf(stderr, "invalid cmd: ");
      print_parser_errcode(stderr, parsedValue);
      continue;
    }

    // If parser returned num_commands = 0, skip
    if (parsedResult->num_commands == 0) {
      free(parsedResult);
      continue;
    }

    // Check builtin
    if (handle_builtin(parsedResult)) {
      free(parsedResult);
      continue;
    }

    // execute the parsed result/command here
    execute(parsedResult, interactive_mode);

    // only free command if it is not a background job
    if (!parsedResult->is_background) {
      free(parsedResult);
    }
  }
  vec_destroy(&job_list);
  free(buffer);
  return 0;
}
