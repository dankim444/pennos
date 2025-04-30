/* CS5480 PennOS Group 61
 * Authors: Dan Kim and Kevin Zhou
 * Purpose: Defines the kernel-level file system functions.
 */

#ifndef FS_KFUNCS_H_
#define FS_KFUNCS_H_

#include <stddef.h>
#include "fat_routines.h"

////////////////////////////////////////////////////////////////////////////////
//                   KERNEL-RELATED FILE SYSTEM FUNCTIONS                     //
////////////////////////////////////////////////////////////////////////////////

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/**
 * @brief Opens a file with the specified mode.
 *
 * This is a kernel-level function that opens a file and returns a file
 * descriptor. The file is created if it doesn't exist and the mode includes
 * F_WRITE. If the file exists and F_APPEND is specified, the file position is
 * set to the end.
 *
 * @param fname The name of the file to open.
 * @param mode  A combination of F_READ, F_WRITE, and F_APPEND.
 *
 * @return A non-negative file descriptor on success, -1 on error with P_ERRNO
 *         set. Possible error codes:
 *         - P_ENOENT: File doesn't exist and F_READ only.
 *         - P_EFULL: Cannot create file (file system full).
 *         - P_EINVAL: Invalid mode or filename.
 */
int k_open(const char* fname, int mode);

/**
 * @brief Reads data from an open file.
 *
 * This is a kernel-level function that reads up to n bytes from an open file
 * into the provided buffer. The file position is advanced by the number of
 * bytes read.
 *
 * @param fd File descriptor of the open file.
 * @param buf Buffer to store the read data.
 * @param n  Maximum number of bytes to read.
 *
 * @return The number of bytes read on success, -1 on error with P_ERRNO set.
 *         Possible error codes:
 *         - P_EBADF: Invalid file descriptor.
 *         - P_EINVAL: Invalid buffer or count.
 */
int k_read(int fd, char* buf, int n);

/**
 * @brief Writes data to an open file.
 *
 * This is a kernel-level function that writes n bytes from the provided buffer
 * to an open file. The file position is advanced by the number of bytes
 * written. If necessary, the file is extended.
 *
 * @param fd  File descriptor of the open file.
 * @param str Buffer containing the data to write.
 * @param n   Number of bytes to write.
 *
 * @return The number of bytes written on success, -1 on error with P_ERRNO set.
 *         Possible error codes:
 *         - P_EBADF: Invalid file descriptor.
 *         - P_EINVAL: Invalid buffer or count.
 *         - P_EFULL: File system is full.
 */
int k_write(int fd, const char* str, int n);

/**
 * @brief Closes an open file.
 *
 * This is a kernel-level function that closes an open file and releases the
 * associated file descriptor. Any unsaved changes are flushed to disk.
 *
 * @param fd File descriptor of the open file.
 *
 * @return 0 on success, -1 on error with P_ERRNO set.
 *         Possible error codes:
 *         - P_EBADF: Invalid file descriptor.
 */
int k_close(int fd);

/**
 * @brief Removes a file from the file system.
 *
 * This is a kernel-level function that deletes the specified file from the
 * file system. The file must not be open by any process.
 *
 * @param fname The name of the file to remove.
 *
 * @return 0 on success, -1 on error with P_ERRNO set.
 *         Possible error codes:
 *         - P_ENOENT: File doesn't exist.
 *         - P_EBUSY: File is still open by some process.
 */
int k_unlink(const char* fname);

/**
 * @brief Repositions the file offset of an open file.
 *
 * This is a kernel-level function that changes the current position within an
 * open file. The interpretation of the offset depends on the whence parameter.
 *
 * @param fd     File descriptor of the open file.
 * @param offset The offset in bytes to set the position to.
 * @param whence How to interpret the offset:
 *               - SEEK_SET (0): Offset is from the beginning of the file.
 *               - SEEK_CUR (1): Offset is from the current position.
 *               - SEEK_END (2): Offset is from the end of the file.
 *
 * @return The new offset location on success, -1 on error with P_ERRNO set.
 *         Possible error codes:
 *         - P_EBADF: Invalid file descriptor.
 *         - P_EINVAL: Invalid whence or the resulting position would be
 *                    negative.
 */
int k_lseek(int fd, int offset, int whence);

/**
 * @brief Lists files or file information.
 *
 * This is a kernel-level function that provides directory listing
 * functionality. If filename is NULL or refers to a directory, it lists all
 * files in that directory. If filename refers to a specific file, it displays
 * detailed information about that file.
 *
 * @param filename The name of the file or directory to list, or NULL for
 *                 the current directory.
 *
 * @return 0 on success, -1 on error with P_ERRNO set.
 *         Possible error codes:
 *         - P_ENOENT: Specified file or directory doesn't exist.
 */
int k_ls(const char* filename);

#endif
