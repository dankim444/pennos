/* CS5480 PennOS Group 61
 * Authors: Dan Kim
 * Purpose: Defines a custom errno for PennOS
 */

#include "./pennos-errno.h"

int P_ERRNO = 0;

/*
intended behavior in any pennos system call, if an error occurs, set P_ERRNO like this and return -1

P_ERRNO = P_ENOENT;
return -1;

When the system call raises an error, the shell should catch it like this:

int fd = s_open("missing.txt", F_READ);
if (fd < 0) {
    u_perror("s_open failed");
}

u_perror will be implemented user_utils.c
*/
