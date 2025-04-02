/* CS5480 PennOS Group 61
 * Authors: Dan Kim
 * Purpose: Implements various utility functions at the user level.
 */

#include "penn_errno.h"

/*
 * Creates a user-level error message similar to perror.
 *
 * @param msg A string representing the error message from the shell.
 */
void u_perror(const char *msg) {
    fprintf(stderr, "%s: ", msg);
    switch (P_ERRNO) {
        case P_ENOENT: fprintf(stderr, "No such file or directory\n"); break;
        case P_EBADF: fprintf(stderr, "Bad file descriptor\n"); break;
        case P_EPERM: fprintf(stderr, "Operation not permitted\n"); break;
        case P_EINVAL: fprintf(stderr, "Invalid argument\n"); break;
        case P_EEXIST: fprintf(stderr, "File already exists\n"); break;
        case P_EBUSY: fprintf(stderr, "File is busy or open\n"); break;
        case P_EFULL: fprintf(stderr, "No space left on device\n"); break;
        case P_EUNKNOWN:
        default: fprintf(stderr, "Unknown error\n"); break;
    }
}