#ifndef SIGNALS_H_
#define SIGNALS_H_

#include <stdbool.h>
#include <sys/types.h>

// Set up signal handlers for interactive shell
void setup_signal_handlers(void);

// Restore default signals for child processes
void restore_default_signals(void);

// Helper to forward a signal to the foreground process group
void forward_signal(int sig);

// Set the foreground process group
void set_foreground_pgid(pid_t pgid);

// Get the current foreground process group
pid_t get_foreground_pgid(void);

// Clear the foreground process group
void clear_foreground_pgid(void);

#endif
