#include "fs_kfuncs.h"
#include "../kernel/kern_pcb.h"
#include "../lib/pennos-errno.h"
#include "fat_routines.h"
#include "fs_helpers.h"
#include "fs_syscalls.h"  // F_READ, F_WRITE, F_APPEND, STDIN_FILENO, STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h> 
#include <sys/types.h>
#include <sys/stat.h> 
#include <errno.h>
#include <stdbool.h> 

// Global file descriptor table
// sys_fd_entry_t sys_fd_table[MAX_GLOBAL_FD];

/**
* Kernel-level call to open a file.
*/
int k_open(const char* fname, int mode) {
    // validate fname and mode
    if (fname == NULL || *fname == '\0') {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    if ((mode & (F_READ | F_WRITE | F_APPEND)) == 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }

    // TODO: this might be redundant since the helper functions in 
    // fs_helpers already checks this before calling any k_ functions
    // check if the file system is mounted
    if (!is_mounted) {
        P_ERRNO = P_FS_NOT_MOUNTED;
        return -1;
    }

    // get a free file descriptor
    int fd = get_free_fd(fd_table);
    if (fd < 0) {
        P_ERRNO = P_EFULL; // no free file descriptors
        return -1;
    }

    // check if the file exists
    dir_entry_t entry;
    int file_offset = find_file(fname, &entry);

    // file exists
    if (file_offset >= 0) {
        
        // check if the file is already open in write mode by another descriptor
        if ((mode & (F_WRITE | F_APPEND)) != 0) {
            for (int i = 0; i < MAX_FDS; i++) {
                if (i != fd && fd_table[i].in_use && strcmp(fd_table[i].filename, fname) == 0 && 
                    (fd_table[i].mode & (F_WRITE | F_APPEND)) != 0) {
                    P_ERRNO = P_EBUSY; // file is already open for writing
                    return -1;
                }
            }
        }
        
        // fill in the file descriptor entry
        fd_table[fd].in_use = 1;
        strncpy(fd_table[fd].filename, fname, 31);
        fd_table[fd].filename[31] = '\0'; // ensure null termination
        fd_table[fd].size = entry.size;
        fd_table[fd].first_block = entry.firstBlock;
        fd_table[fd].mode = mode;
        
        // set the initial position
        if (mode & F_APPEND) {
            // position at the end of file for append mode
            fd_table[fd].position = entry.size;
        } else {
            // position at the beginning of file for other modes
            fd_table[fd].position = 0;
        }
        
        // if mode includes F_WRITE and not F_APPEND, truncate the file
        if ((mode & F_WRITE) && !(mode & F_APPEND)) {
            // free all blocks except the first one
            uint16_t block = entry.firstBlock;
            uint16_t next_block;
            
            if (block != 0 && block != FAT_EOF) {
                next_block = fat[block];
                fat[block] = FAT_EOF; // terminate the chain at the first block
                block = next_block;
                
                // free the rest of the chain
                while (block != 0 && block != FAT_EOF) {
                    next_block = fat[block];
                    fat[block] = FAT_FREE;
                    block = next_block;
                }
            }
            
            // update file size to 0
            fd_table[fd].size = 0;
            
            // update the directory entry
            entry.size = 0;
            entry.mtime = time(NULL);
            
            if (lseek(fs_fd, fat_size + file_offset, SEEK_SET) != -1) {
                write(fs_fd, &entry, sizeof(entry));
            }
        }
    } else {
        // file doesn't exist
        
        // check if we can create it
        if (!(mode & F_WRITE)) {
            P_ERRNO = P_ENOENT; // file doesn't exist and read-only mode
            return -1;
        }
        
        // allocate the first block
        uint16_t first_block = allocate_block();
        if (first_block == 0) {
            P_ERRNO = P_EFULL; // no free blocks
            return -1;
        }
        
        // create a new file entry
        if (add_file_entry(fname, 0, first_block, TYPE_REGULAR, PERM_READ_WRITE) == -1) {
            // error adding file entry, free the allocated block
            fat[first_block] = FAT_FREE;
            return -1; // error code already set by add_file_entry
        }
        
        // fill in the file descriptor entry
        fd_table[fd].in_use = 1;
        strncpy(fd_table[fd].filename, fname, 31);
        fd_table[fd].filename[31] = '\0'; // ensure null termination
        fd_table[fd].size = 0;
        fd_table[fd].first_block = first_block;
        fd_table[fd].position = 0;
        fd_table[fd].mode = mode;
    }

    return fd;
}

/**
* Kernel-level call to read a file.
*/
int k_read(int fd, int n, char *buf) {
    // validate the file descriptor
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }
    
    // validate the buffer and count
    if (buf == NULL || n < 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    
    // if n is 0, nothing to read
    if (n == 0) {
        return 0;
    }
    
    // check if we're at EOF already
    if (fd_table[fd].position >= fd_table[fd].size) {
        return 0;  // EOF
    }
    
    // determine how many bytes we can actually read
    uint32_t bytes_to_read = n;
    if (fd_table[fd].position + bytes_to_read > fd_table[fd].size) {
        bytes_to_read = fd_table[fd].size - fd_table[fd].position;
    }
    
    // find the block containing the current position
    uint16_t current_block = fd_table[fd].first_block;
    uint32_t block_index = fd_table[fd].position / block_size;
    uint32_t block_offset = fd_table[fd].position % block_size;
    
    // navigate to the correct block in the chain
    for (uint32_t i = 0; i < block_index; i++) {
        if (current_block == 0 || current_block == FAT_EOF) {
            // unexpected end of chain
            P_ERRNO = P_EINVAL;
            return -1;
        }
        current_block = fat[current_block];
    }
    
    // now we're at the right block, start reading
    uint32_t bytes_read = 0;
    
    while (bytes_read < bytes_to_read) {
        // how much data can we read from the current block
        uint32_t bytes_left_in_block = block_size - block_offset;
        uint32_t bytes_to_read_now = (bytes_to_read - bytes_read) < bytes_left_in_block ?
                                    (bytes_to_read - bytes_read) : bytes_left_in_block;
        
        // seek to the right position in the file
        if (lseek(fs_fd, fat_size + (current_block - 1) * block_size + block_offset, SEEK_SET) == -1) {
            P_ERRNO = P_LSEEK;
            // if we already read some data, return that count
            if (bytes_read > 0) {
                fd_table[fd].position += bytes_read;
                return bytes_read;
            }
            return -1;
        }
        
        // read the data from the file
        ssize_t read_result = read(fs_fd, buf + bytes_read, bytes_to_read_now);
        if (read_result <= 0) {
            P_ERRNO = P_READ;
            // if we already read some data, return that count
            if (bytes_read > 0) {
                fd_table[fd].position += bytes_read;
                return bytes_read;
            }
            return -1;
        }
        
        bytes_read += read_result;
        block_offset += read_result;
        
        // if we've read all data from this block and still have more to read, go to the next block
        if (block_offset == block_size && bytes_read < bytes_to_read) {
            if (current_block == FAT_EOF) {
                // unexpected end of chain
                break;
            }
            current_block = fat[current_block];
            block_offset = 0;
        }
        
        // if we read less than expected, we might have hit EOF
        if (read_result < bytes_to_read_now) {
            break;
        }
    }
    
    // update file position
    fd_table[fd].position += bytes_read;
    
    return bytes_read;
}

/**
 * Kernel-level call to write to a file.
 */
 int k_write(int fd, const char* str, int n) {
    // Validate inputs
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }
    if (str == NULL || n < 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    
    // Check if filesystem is mounted and FAT is valid
    if (!is_mounted || fat == NULL) {
        P_ERRNO = P_FS_NOT_MOUNTED;
        return -1;
    }
    
    // Get file information
    uint16_t current_block = fd_table[fd].first_block;
    uint32_t current_position = fd_table[fd].position;
    
    // Create a local buffer for block data
    char* block_buffer = malloc(block_size);
    if (block_buffer == NULL) {
        P_ERRNO = P_EMALLOC;
        return -1;
    }
    
    // Calculate initial block position
    uint32_t block_index = current_position / block_size;
    uint32_t block_offset = current_position % block_size;
    
    // If the file doesn't have a first block yet, allocate one
    if (current_block == 0) {
        current_block = allocate_block();
        if (current_block == 0) {
            P_ERRNO = P_EFULL;
            free(block_buffer);
            return -1;
        }
        fd_table[fd].first_block = current_block;
    }
    
    // Navigate to the appropriate block
    uint16_t prev_block = 0;
    for (uint32_t i = 0; i < block_index; i++) {
        if (current_block == 0 || current_block == FAT_EOF || current_block >= fat_size / 2) {
            // Reached the end of chain prematurely, need to allocate a new block
            uint16_t new_block = allocate_block();
            if (new_block == 0) {
                P_ERRNO = P_EFULL;
                free(block_buffer);
                return -1;
            }
            
            // Update the chain
            if (prev_block != 0 && prev_block < fat_size / 2) {
                fat[prev_block] = new_block;
            } else {
                // If there's no previous block, this must be the first one
                fd_table[fd].first_block = new_block;
            }
            
            current_block = new_block;
        }
        
        prev_block = current_block;
        
        // Validate the block number before accessing FAT
        if (current_block >= fat_size / 2) {
            P_ERRNO = P_EINVAL;
            free(block_buffer);
            return -1;
        }
        
        current_block = fat[current_block];
    }
    
    // If we ended up without a valid block, go back to the last valid one
    if (current_block == 0 || current_block == FAT_EOF || current_block >= fat_size / 2) {
        if (prev_block != 0 && prev_block < fat_size / 2) {
            uint16_t new_block = allocate_block();
            if (new_block == 0) {
                P_ERRNO = P_EFULL;
                free(block_buffer);
                return -1;
            }
            
            fat[prev_block] = new_block;
            current_block = new_block;
        } else {
            // This really shouldn't happen if we've allocated a first block already
            P_ERRNO = P_EINVAL;
            free(block_buffer);
            return -1;
        }
    }
    
    // Start writing data
    uint32_t bytes_written = 0;
    
    while (bytes_written < n) {
        // Validate current block
        if (current_block == 0 || current_block == FAT_EOF || current_block >= fat_size / 2) {
            P_ERRNO = P_EINVAL;
            break;
        }
        
        // How much can we write to this block
        uint32_t space_in_block = block_size - block_offset;
        uint32_t bytes_to_write = (n - bytes_written) < space_in_block ? 
                                  (n - bytes_written) : space_in_block;
        
        // Position in filesystem
        off_t block_position = fat_size + (current_block - 1) * block_size;
        
        // If we're not writing a full block or not starting at the beginning, 
        // we need to read-modify-write
        if (bytes_to_write < block_size || block_offset > 0) {
            // Read the current block
            if (lseek(fs_fd, block_position, SEEK_SET) == -1) {
                P_ERRNO = P_LSEEK;
                break;
            }
            
            // Read the current block data
            ssize_t read_result = read(fs_fd, block_buffer, block_size);
            if (read_result < 0) {
                P_ERRNO = P_READ;
                break;
            }
            
            // Copy the new data into the block buffer
            memcpy(block_buffer + block_offset, str + bytes_written, bytes_to_write);
            
            // Seek back to write the modified block
            if (lseek(fs_fd, block_position, SEEK_SET) == -1) {
                P_ERRNO = P_LSEEK;
                break;
            }
            
            // Write the full block back
            ssize_t write_result = write(fs_fd, block_buffer, block_size);
            if (write_result != block_size) {
                P_ERRNO = P_EINVAL;
                // We might have a partial write, but that's hard to handle correctly
                break;
            }
        } else {
            // We're writing a full block from the beginning
            if (lseek(fs_fd, block_position, SEEK_SET) == -1) {
                P_ERRNO = P_LSEEK;
                break;
            }
            
            ssize_t write_result = write(fs_fd, str + bytes_written, bytes_to_write);
            if (write_result != bytes_to_write) {
                P_ERRNO = P_EINVAL;
                break;
            }
        }
        
        // Update counters
        bytes_written += bytes_to_write;
        block_offset = (block_offset + bytes_to_write) % block_size;
        
        // If we've filled this block and still have more to write, go to the next block
        if (block_offset == 0 && bytes_written < n) {
            // Validate current block before accessing FAT
            if (current_block >= fat_size / 2) {
                P_ERRNO = P_EINVAL;
                break;
            }
            
            // Check if there's a next block
            if (fat[current_block] == FAT_EOF) {
                // Allocate a new block
                uint16_t new_block = allocate_block();
                if (new_block == 0) {
                    P_ERRNO = P_EFULL;
                    break;
                }
                
                // Update the FAT safely
                if (current_block < fat_size / 2) {
                    fat[current_block] = new_block;
                } else {
                    P_ERRNO = P_EINVAL;
                    break;
                }
                
                current_block = new_block;
            } else {
                current_block = fat[current_block];
            }
        }
    }
    
    // Free the block buffer
    free(block_buffer);
    
    // Update file position
    fd_table[fd].position += bytes_written;
    
    // Update file size if needed
    if (fd_table[fd].position > fd_table[fd].size) {
        fd_table[fd].size = fd_table[fd].position;
        
        // Update the directory entry
        dir_entry_t entry;
        int dir_offset = find_file(fd_table[fd].filename, &entry);
        if (dir_offset >= 0) {
            entry.size = fd_table[fd].size;
            entry.mtime = time(NULL);
            
            if (lseek(fs_fd, fat_size + dir_offset, SEEK_SET) != -1) {
                write(fs_fd, &entry, sizeof(entry));
            }
        }
    }
    
    return bytes_written;
}

/**
* Kernel-level call to close a file.
*/
int k_close(int fd) {
    // validate the file descriptor
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }
    
    // ensure any pending changes are written to disk
    // update the directory entry with the current file size
    dir_entry_t entry;
    int file_offset = find_file(fd_table[fd].filename, &entry);
    
    if (file_offset >= 0) {
        // update file size if it changed
        if (entry.size != fd_table[fd].size) {
            entry.size = fd_table[fd].size;
            entry.mtime = time(NULL);
            
            if (lseek(fs_fd, fat_size + file_offset, SEEK_SET) != -1) {
                write(fs_fd, &entry, sizeof(entry));
            }
        }
    }
    
    // mark the file descriptor as not in use
    fd_table[fd].in_use = 0;
    
    return 0;
}

/**
 * Kernel-level call to remove a file.
 */
int k_unlink(const char* fname) {
  return 0;
}

/**
* Kernel-level call to re-position a file offset.
*/
int k_lseek(int fd, int offset, int whence) {
    // validate the file descriptor
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }

    // calculate new position based on whence
    int32_t new_position;

    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = fd_table[fd].position + offset;
            break;
        case SEEK_END:
            new_position = fd_table[fd].size + offset;
            break;
        default:
            P_ERRNO = P_EINVAL;
            return -1;
    }

    // check if new position is valid
    if (new_position < 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }

    // update file position
    fd_table[fd].position = new_position;

    return new_position;
}

/**
* Kernel-level call to list files.
*/
int k_ls(const char* filename) {
  return 0;
}
