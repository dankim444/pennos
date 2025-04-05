#ifndef SIGNAL_H_
#define SIGNAL_H_

// signal definitions
#define P_SIGSTOP 10
#define P_SIGCONT 11
#define P_SIGTERM 12

// status definitions
#define EXITED_NORMALLY 20
#define STOPPED_BY_SIG 21
#define TERM_BY_SIG 22

// user-level macros for waitpid status
#define P_WIFEXITED(status) ((status) == EXITED_NORMALLY)
#define P_WIFSTOPPED(status) ((status) == STOPPED_BY_SIG)
#define P_WIFSIGNALED(status) ((status) == TERM_BY_SIG)


#endif