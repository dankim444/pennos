#ifndef PENNOS_ERRNO_H
#define PENNOS_ERRNO_H

// TODO: rework some of the variable names

extern int P_ERRNO; // Global errno variable that is set to different 
                    // error codes based on what error occurs in PennOS system calls.

#define P_ENOENT 1 // No such file or directory
#define P_EBADF 2 // Bad file descriptor
#define P_EPERM 3 // Permission invalid
#define P_EINVAL 4 // Invalid argument
#define P_EEXIST 5 // File already exists
#define P_EBUSY 6 // File is busy (in use)
#define P_EFULL 7 // No space left / FAT full
#define P_EFS_NOT_MOUNTED 8 // File system not mounted
#define P_EINTR 9 // Interrupted system call (general purpose)
#define P_ENULL 10  // Undesired NULL output
#define P_EREAD 11 // Interrupted READ call
#define P_ELSEEK 12 // Interrupted LSEEK call
#define P_EMAP 13 // Interrupted MAP call
#define P_EFUNC 14 // Error comes from a function
#define P_EOPEN 15 // Error when trying to open a file
#define P_EMALLOC 16 // Error when trying to malloc
#define P_ESIGNAL 17 // Error with sigaction
#define P_EWRITE 18 // Error when trying to write
#define P_ECLOSE 19 // Error when trying to close a file
#define P_EPARSE 20 // Error when trying to parse a command
#define P_ECOMMAND 21 // Command not found
#define P_EUNKNOWN 99 // Catch-all unknown error

#endif
