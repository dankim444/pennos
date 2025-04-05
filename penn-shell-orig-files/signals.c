/* CS5480 Project 02 penn-shell
 * Authors: Dan Kim and Richard Zhang
 * Purpose: Code for setting up and handling signals.
 */

#include "signals.h"
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern bool interactive_mode;  // from penn-shell.c
static pid_t fg_pgid = 0;      // Track foreground process group

// Forward a signal to the foreground process group
void forward_signal(int sig) {
  if (fg_pgid > 0) {
    kill(-fg_pgid, sig);
  }
}

// Set the foreground process group
void set_foreground_pgid(pid_t pgid) {
  fg_pgid = pgid;
}

// Get the current foreground process group
pid_t get_foreground_pgid(void) {
  return fg_pgid;
}

// Clear the foreground process group
void clear_foreground_pgid(void) {
  fg_pgid = 0;
}

// Handle Ctrl-C (SIGINT)
void handle_sigint(int sig) {
  if (sig == SIGINT) {
    if (interactive_mode) {
      forward_signal(SIGINT);  // Forward signal to any foreground job
      if (write(STDERR_FILENO, "\n", 1) == -1) {
        perror("Write ctrl-c new line, error");
      }
    } else {
      // Ctrl-C terminates shell in non-interactive mode
      _exit(0);
    }
  }
}

// Handle Ctrl-Z (SIGTSTP)
void handle_sigstop(int sig) {
  if (sig == SIGTSTP) {
    if (interactive_mode) {
      forward_signal(SIGTSTP);  // Forward signal to any foreground job
      if (write(STDERR_FILENO, "\n", 1) == -1) {
        perror("Write ctrl-z new line, error");
      }
    } else {
      // Ctrl-Z stops shell in non-interactive mode
      kill(getpid(), SIGSTOP);
    }
  }
}

// An interactive penn-shell should never stop or exit because of the following
// signals: SIGTTIN, SIGTTOU, SIGINT, SIGQUIT, SIGTSTP
void setup_signal_handlers(void) {
  struct sigaction sa_int = {0};
  sa_int.sa_handler = handle_sigint;
  sa_int.sa_flags = 0;
  sigemptyset(&sa_int.sa_mask);
  if (sigaction(SIGINT, &sa_int, NULL) == -1) {
    perror("sigaction for SIGINT error");
  }

  struct sigaction sa_stop = {0};
  sa_stop.sa_handler = handle_sigstop;
  sa_stop.sa_flags = 0;
  sigemptyset(&sa_stop.sa_mask);
  if (sigaction(SIGTSTP, &sa_stop, NULL) == -1) {
    perror("sigaction for SIGTSTP error");
  }

  struct sigaction sa_ign = {0};
  sa_ign.sa_handler = SIG_IGN;
  sa_ign.sa_flags = 0;
  sigemptyset(&sa_ign.sa_mask);
  if (sigaction(SIGTTIN, &sa_ign, NULL) == -1) {
    perror("sigaction for SIGTTIN error");
  }
  if (sigaction(SIGTTOU, &sa_ign, NULL) == -1) {
    perror("sigaction for SIGTTOU error");
  }
  if (sigaction(SIGQUIT, &sa_ign, NULL) == -1) {
    perror("sigaction for SIGQUIT error");
  }
}

// Restore default signals for child processes
void restore_default_signals(void) {
  struct sigaction sa_dfl = {0};
  sa_dfl.sa_handler = SIG_DFL;
  sa_dfl.sa_flags = 0;
  sigemptyset(&sa_dfl.sa_mask);

  if (sigaction(SIGINT, &sa_dfl, NULL) == -1) {
    perror("sigaction for SIGINT error");
  }
  if (sigaction(SIGTSTP, &sa_dfl, NULL) == -1) {
    perror("sigaction for SIGTSTP error");
  }
  if (sigaction(SIGTTIN, &sa_dfl, NULL) == -1) {
    perror("sigaction for SIGTTIN error");
  }
  if (sigaction(SIGTTOU, &sa_dfl, NULL) == -1) {
    perror("sigaction for SIGTTOU error");
  }
  if (sigaction(SIGQUIT, &sa_dfl, NULL) == -1) {
    perror("sigaction for SIGQUIT error");
  }
  if (sigaction(SIGCHLD, &sa_dfl, NULL) == -1) {
    perror("sigaction for SIGCHLD error");
  }
}
