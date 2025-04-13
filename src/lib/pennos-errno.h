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
#define P_FS_NOT_MOUNTED 8 // File system not mounted
#define P_EINTR 8 // Interrupted system call (general purpose)
#define P_NULL 9  // Undesired NULL output
#define P_READ 10 // Interrupted READ call
#define P_LSEEK 11 // Interrupted LSEEK call
#define P_MAP 12 // Interrupted MAP call
#define P_EFUNC 13 // Error comes from a custom function
#define P_EUNKNOWN 99 // Catch-all unknown error

#endif
