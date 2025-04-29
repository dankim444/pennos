/* CS5480 PennOS Group 61
 * Authors: Dan Kim and Kevin Zhou
 * Purpose: Implements the system-level file system calls.
 */

#include "fs_syscalls.h"
#include "fs_kfuncs.h"
#include "../lib/pennos-errno.h"

/**
 * @brief System call to open a file.
 * 
 * This is a wrapper around the kernel function k_open.
 */
int s_open(const char *fname, int mode) {
    return k_open(fname, mode);
}

/**
 * @brief System call to read from a file.
 * 
 * This is a wrapper around the kernel function k_read.
 */
int s_read(int fd, char *buf, int n) {
    return k_read(fd, buf, n);
}

/**
 * @brief System call to write to a file.
 * 
 * This is a wrapper around the kernel function k_write.
 */
int s_write(int fd, const char *str, int n) {
    return k_write(fd, str, n);
}

/**
 * @brief System call to close a file.
 * 
 * This is a wrapper around the kernel function k_close.
 */
int s_close(int fd) {
    return k_close(fd);
}

/**
 * @brief System call to remove a file.
 * 
 * This is a wrapper around the kernel function k_unlink.
 */
int s_unlink(const char *fname) {
    return k_unlink(fname);
}

/**
 * @brief System call to reposition the file offset.
 * 
 * This is a wrapper around the kernel function k_lseek.
 */
int s_lseek(int fd, int offset, int whence) {
    return k_lseek(fd, offset, whence);
}

/**
 * @brief System call to list files.
 * 
 * This is a wrapper around the kernel function k_ls.
 */
int s_ls(const char *filename) {
    return k_ls(filename);
}