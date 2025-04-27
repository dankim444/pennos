#ifndef FS_SYS_CALLS_H_
#define FS_SYS_CALLS_H_

#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
//       SYSTEM-LEVEL FILE SYSTEM VARIABLES (USER-ACCESSIBLE)                 //
////////////////////////////////////////////////////////////////////////////////

// Reserved standard file descriptors
#define STDIN_FILENO 0  // Standard input
#define STDOUT_FILENO 1  // Standard output
#define STDERR_FILENO 2  // Standard error

////////////////////////////////////////////////////////////////////////////////
//                     SYSTEM-LEVEL FILE SYSTEM CALLS                          //
////////////////////////////////////////////////////////////////////////////////

/**
* @brief Opens a file with the specified access mode.
* 
* This function provides a user-level interface to the kernel's file open operation.
* It opens the specified file with the given access mode and returns a file descriptor
* that can be used in subsequent operations on the file.
*
* @param fname The name of the file to open.
* @param mode A combination of F_READ, F_WRITE, and F_APPEND.
*
* @return On success, returns a non-negative integer representing the file descriptor.
*         On error, returns -1 and sets P_ERRNO appropriately:
*         - P_ENOENT: The file does not exist and F_READ was specified.
*         - P_EINVAL: Invalid parameters (NULL filename or invalid mode).
*         - P_EFULL: No space left on device or file descriptor table is full.
*/
int s_open(const char *fname, int mode);

/**
* @brief Reads data from an open file.
* 
* This function reads up to n bytes from the file associated with the file descriptor fd
* into the buffer starting at buf. The file offset is advanced by the number of bytes read.
*
* @param fd The file descriptor of an open file.
* @param n The maximum number of bytes to read.
* @param buf The buffer to store the read data.
*
* @return On success, returns the number of bytes read (0 indicates end of file).
*         On error, returns -1 and sets P_ERRNO appropriately:
*         - P_EBADF: fd is not a valid file descriptor or is not open for reading.
*         - P_EINVAL: Invalid parameters (NULL buffer or negative count).
*/
int s_read(int fd, char *buf, int n);

/**
* @brief Writes data to an open file.
* 
* This function writes up to n bytes from the buffer starting at str to the file
* associated with the file descriptor fd. The file offset is advanced by the number
* of bytes written.
*
* @param fd The file descriptor of an open file.
* @param str The buffer containing the data to be written.
* @param n The number of bytes to write.
*
* @return On success, returns the number of bytes written.
*         On error, returns -1 and sets P_ERRNO appropriately:
*         - P_EBADF: fd is not a valid file descriptor or is not open for writing.
*         - P_EINVAL: Invalid parameters (NULL buffer or negative count).
*         - P_EFULL: No space left on device.
*/
int s_write(int fd, const char *str, int n);

/**
* @brief Closes an open file descriptor.
* 
* This function closes the file descriptor fd, making it available for reuse.
* If this is the last reference to the underlying file, any necessary cleanup
* is performed.
*
* @param fd The file descriptor to close.
*
* @return On success, returns 0.
*         On error, returns -1 and sets P_ERRNO appropriately:
*         - P_EBADF: fd is not a valid file descriptor.t
*/
int s_close(int fd);

/**
* @brief Removes a file from the file system.
* 
* This function removes the specified file from the file system.
* If the file is currently open, the behavior depends on the implementation.
*
* @param fname The name of the file to remove.
*
* @return On success, returns 0.
*         On error, returns -1 and sets P_ERRNO appropriately:
*         - P_ENOENT: The file does not exist.
*         - P_EBUSY: The file is currently in use.
*         - P_EINVAL: Invalid parameter (NULL filename).
*/
int s_unlink(const char *fname);

/**
* @brief Repositions the file offset of an open file.
* 
* This function repositions the offset of the file descriptor fd to the argument offset
* according to the directive whence.
*
* @param fd The file descriptor of an open file.
* @param offset The offset in bytes.
* @param whence Specifies the reference position:
*               0 (SEEK_SET): The offset is set relative to the start of the file.
*               1 (SEEK_CUR): The offset is set relative to the current position.
*               2 (SEEK_END): The offset is set relative to the end of the file.
*
* @return On success, returns the resulting offset from the beginning of the file.
*         On error, returns -1 and sets P_ERRNO appropriately:
*         - P_EBADF: fd is not a valid file descriptor.
*         - P_EINVAL: whence is not valid or the resulting offset would be negative.
*/
int s_lseek(int fd, int offset, int whence);

/**
* @brief Lists files in the current directory or displays file information.
* 
* If filename is NULL, this function lists all files in the current directory.
* If filename refers to a specific file, it displays detailed information about that file.
*
* @param filename The name of the file to get information about, or NULL to list all files.
*
* @return On success, returns 0.
*         On error, returns -1 and sets P_ERRNO appropriately:
*         - P_ENOENT: The specified file does not exist.
*/
int s_ls(const char *filename);


#endif