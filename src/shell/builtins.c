/* CS5480 PennOS Group 61
 * Authors: Dan Kim
 * Purpose: Implements various utility functions at the user level.
 */
#include <stdio.h>
#include "builtins.h"
#include "../lib/pennos-errno.h"

void u_perror(const char *msg) {
    char buffer[256];  // adjust size if we need
    const char *error_msg;

    switch (P_ERRNO) {
        case P_ENOENT: 
            error_msg = "no such file or directory"; 
            break;
        case P_EBADF: 
            error_msg = "bad file descriptor"; 
            break;
        case P_EPERM: 
            error_msg = "operation not permitted"; 
            break;
        case P_EINVAL: 
            error_msg = "invalid arg"; 
            break;
        case P_EEXIST: 
            error_msg = "file already exists"; 
            break;
        case P_EBUSY: 
            error_msg = "file is busy or open"; 
            break;
        case P_EFULL: 
            error_msg = "no space left on device"; 
            break;
        case P_EINTR:
            error_msg = "interrupted system call"; 
            break;
        case P_NULL:
            error_msg = "NULL returned unexpectedly"; 
            break;
        case P_EUNKNOWN:
            error_msg = "unknown error"; 
            break;
        case P_READ:
            error_msg = "interrupted read call"; 
            break;
        case P_LSEEK:
            error_msg = "interrupted lseek call"; 
            break;
        case P_MAP:
            error_msg = "interrupted mmap/munmap call"; 
            break;
        case P_EFUNC:
            error_msg = "interrupted custom call";
            break;
        case P_EOPEN:
            error_msg = "interrupted open call";
            break;
        case P_EMALLOC:
            error_msg = "error when trying to malloc";
            break;
        case P_FS_NOT_MOUNTED:
            error_msg = "file system not mounted yet";
            break;
        default: 
            error_msg = "Unknown error"; 
            break;
    }

    snprintf(buffer, sizeof(buffer), "%s: %s\n", msg, error_msg);
    fputs(buffer, stderr); // TODO --> check if this is allowed
}