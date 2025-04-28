#include "fs_kfuncs.h"
#include "../kernel/kern_pcb.h"
#include "../lib/pennos-errno.h"
#include "fat_routines.h"
#include "fs_helpers.h"
#include "fs_syscalls.h"  // F_READ, F_WRITE, F_APPEND, STDIN_FILENO, STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO
#include "../kernel/kern_sys_calls.h"
#include "../kernel/signal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h> 
#include <sys/types.h>
#include <errno.h>
#include <stdbool.h> 

extern pcb_t* current_running_pcb;
extern pid_t current_fg_pid;

/**
* Kernel-level call to open a file.
*/
int k_open(const char* fname, int mode) {
    // validate arguments
    if (fname == NULL || *fname == '\0') {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    if ((mode & (F_READ | F_WRITE | F_APPEND)) == 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }

    // check if the file system is mounted
    if (!is_mounted) {
        P_ERRNO = P_EFS_NOT_MOUNTED;
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
        fd_table[fd].ref_count++;
        strncpy(fd_table[fd].filename, fname, 31);
        fd_table[fd].filename[31] = '\0';
        fd_table[fd].size = entry.size;
        fd_table[fd].first_block = entry.firstBlock;
        fd_table[fd].mode = mode;
        
        // set the initial position
        if (mode & F_APPEND) {
            fd_table[fd].position = entry.size;
        } else {
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
            entry.size = 0;
            entry.mtime = time(NULL);
            
            // update the file system with the truncated file
            if (lseek(fs_fd, file_offset, SEEK_SET) == -1) {
                P_ERRNO = P_ELSEEK;
                return -1;
            }
            if (write(fs_fd, &entry, sizeof(entry)) != sizeof(entry)) {
                P_ERRNO = P_EWRITE;
                return -1;
            }
        }
    } else {
        // file doesn't exist
        
        // we can only create it if we are reading the file
        if (!(mode & F_WRITE)) {
            P_ERRNO = P_ENOENT;
            return -1;
        }
        
        // allocate the first block
        uint16_t first_block = allocate_block();
        if (first_block == 0) {
            P_ERRNO = P_EFULL;
            return -1;
        }
        
        // create a new file entry
        if (add_file_entry(fname, 0, first_block, TYPE_REGULAR, PERM_READ_WRITE) == -1) {
            // error code already set by add_file_entry
            fat[first_block] = FAT_FREE;
            return -1;
        }
        
        // fill in the file descriptor entry
        fd_table[fd].in_use = 1;
        fd_table[fd].ref_count++;
        strncpy(fd_table[fd].filename, fname, 31);
        fd_table[fd].filename[31] = '\0';
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
int k_read(int fd, char *buf, int n) {
    // handle terminal control (if doesn't control, send a STOP signal)
    if (fd == STDIN_FILENO && current_running_pcb != NULL) {
        if (current_running_pcb->pid != current_fg_pid) {
            s_kill(current_running_pcb->pid, P_SIGSTOP);
        }
    }

    // handle standard input
    if (fd == STDIN_FILENO) {
        return read(STDIN_FILENO, buf, n);
    }

    // validate inputs
    if (fd < 0 || fd >= MAX_FDS || !fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }
    if (buf == NULL || n < 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    
    // check if we're at EOF already
    if (fd_table[fd].position >= fd_table[fd].size) {
        return 0;
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
            P_ERRNO = P_ELSEEK;
            if (bytes_read > 0) {
                fd_table[fd].position += bytes_read;
                return bytes_read;
            }
            return -1;
        }
        
        // read the data from the file
        ssize_t read_result = read(fs_fd, buf + bytes_read, bytes_to_read_now);
        if (read_result <= 0) {
            P_ERRNO = P_EREAD;
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
    // handle standard output and error
    if (fd == STDOUT_FILENO) {
        return write(STDOUT_FILENO, str, n);
    }
    if (fd == STDERR_FILENO) {
        return write(STDERR_FILENO, str, n);
    }

    // validate inputs
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
    
    // check if filesystem is mounted and FAT is valid
    if (!is_mounted || fat == NULL) {
        P_ERRNO = P_EFS_NOT_MOUNTED;
        return -1;
    }
    
    // get file information
    uint16_t current_block = fd_table[fd].first_block;
    uint32_t current_position = fd_table[fd].position;
    
    // create a local buffer for block data
    char* block_buffer = (char*) malloc(block_size);
    if (block_buffer == NULL) {
        P_ERRNO = P_EMALLOC;
        return -1;
    }
    
    // calculate initial block position
    uint32_t block_index = current_position / block_size;
    uint32_t block_offset = current_position % block_size;
    
    // if the file doesn't have a first block yet, allocate one
    if (current_block == 0) {
        current_block = allocate_block();
        if (current_block == 0) {
            P_ERRNO = P_EFULL;
            free(block_buffer);
            return -1;
        }
        fd_table[fd].first_block = current_block;
    }
    
    // navigate to the appropriate block
    uint16_t prev_block = 0;
    for (uint32_t i = 0; i < block_index; i++) {
        if (current_block == 0 || current_block == FAT_EOF || current_block >= fat_size / 2) {
            // reached the end of chain prematurely, need to allocate a new block
            uint16_t new_block = allocate_block();
            if (new_block == 0) {
                P_ERRNO = P_EFULL;
                free(block_buffer);
                return -1;
            }
            
            // update the chain
            if (prev_block != 0 && prev_block < fat_size / 2) {
                fat[prev_block] = new_block;
            } else {
                // if there's no previous block, this must be the first one
                fd_table[fd].first_block = new_block;
            }
            
            current_block = new_block;
        }
        
        prev_block = current_block;
        
        // validate the block number before accessing FAT
        if (current_block >= fat_size / 2) {
            P_ERRNO = P_EINVAL;
            free(block_buffer);
            return -1;
        }
        
        current_block = fat[current_block];
    }
    
    // if we ended up without a valid block, go back to the last valid one
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
            P_ERRNO = P_EINVAL;
            free(block_buffer);
            return -1;
        }
    }
    
    // start writing data
    uint32_t bytes_written = 0;
    
    while (bytes_written < n) {
        // validate current block
        if (current_block == 0 || current_block == FAT_EOF || current_block >= fat_size / 2) {
            P_ERRNO = P_EINVAL;
            break;
        }
        
        // how much can we write to this block
        uint32_t space_in_block = block_size - block_offset;
        uint32_t bytes_to_write = (n - bytes_written) < space_in_block ? 
                                  (n - bytes_written) : space_in_block;
        
        // position in filesystem
        off_t block_position = fat_size + (current_block - 1) * block_size;
        
        // if we're not writing a full block or not starting at the beginning, we need to read-modify-write
        if (bytes_to_write < block_size || block_offset > 0) {
            // read the current block
            if (lseek(fs_fd, block_position, SEEK_SET) == -1) {
                P_ERRNO = P_ELSEEK;
                break;
            }
            
            // read the current block data
            ssize_t read_result = read(fs_fd, block_buffer, block_size);
            if (read_result < 0) {
                P_ERRNO = P_EREAD;
                break;
            }
            
            // copy the new data into the block buffer
            memcpy(block_buffer + block_offset, str + bytes_written, bytes_to_write);
            
            // seek back to write the modified block
            if (lseek(fs_fd, block_position, SEEK_SET) == -1) {
                P_ERRNO = P_ELSEEK;
                break;
            }
            
            // write the full block back
            ssize_t write_result = write(fs_fd, block_buffer, block_size);
            if (write_result != block_size) {
                P_ERRNO = P_EWRITE;
                // we might have a partial write, but that's hard to handle correctly
                break;
            }
        } else {
            // we're writing a full block from the beginning
            if (lseek(fs_fd, block_position, SEEK_SET) == -1) {
                P_ERRNO = P_ELSEEK;
                break;
            }
            
            ssize_t write_result = write(fs_fd, str + bytes_written, bytes_to_write);
            if (write_result != bytes_to_write) {
                P_ERRNO = P_EWRITE;
                break;
            }
        }
        
        // update counters
        bytes_written += bytes_to_write;
        block_offset = (block_offset + bytes_to_write) % block_size;
        
        // if we've filled this block and still have more to write, go to the next block
        if (block_offset == 0 && bytes_written < n) {
            // validate current block before accessing FAT
            if (current_block >= fat_size / 2) {
                P_ERRNO = P_EINVAL;
                break;
            }
            
            // check if there's a next block
            if (fat[current_block] == FAT_EOF) {
                // allocate a new block
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
    
    // free the block buffer
    free(block_buffer);
    
    // update file position
    fd_table[fd].position += bytes_written;
    
    // update file size if needed
    if (fd_table[fd].position > fd_table[fd].size) {
        fd_table[fd].size = fd_table[fd].position;
        
        // update the directory entry
        dir_entry_t entry;
        int dir_offset = find_file(fd_table[fd].filename, &entry);
        if (dir_offset >= 0) {
            entry.size = fd_table[fd].size;
            entry.mtime = time(NULL);
            
            if (lseek(fs_fd, dir_offset, SEEK_SET) == -1) {
                P_ERRNO = P_ELSEEK;
                return -1;
            }
            if (write(fs_fd, &entry, sizeof(entry)) != sizeof(entry)) {
                P_ERRNO = P_EWRITE;
                return -1;
            }
        }
    }
    
    return bytes_written;
}

/**
* Kernel-level call to close a file.
*/
int k_close(int fd) {

    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
   
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
            
            if (lseek(fs_fd, file_offset, SEEK_SET) != -1) {
                write(fs_fd, &entry, sizeof(entry));
            }
        }
    }
    
    // decrement the reference count
    decrement_fd_ref_count(fd);
    
    return 0;
}

/**
* Kernel-level call to remove a file.
*/
int k_unlink(const char* fname) {
    if (fname == NULL || *fname == '\0') {
        P_ERRNO = P_EINVAL;
        return -1;
    }

    if (!is_mounted) {
        P_ERRNO = P_EFS_NOT_MOUNTED;
        return -1;
    }

    // check if file is currently open by any process
    for (int i = 0; i < MAX_FDS; i++) {
        if (fd_table[i].in_use && strcmp(fd_table[i].filename, fname) == 0) {
            P_ERRNO = P_EBUSY;
            return -1;
        }
    }

    // find the file in directory
    dir_entry_t entry;
    int file_offset = find_file(fname, &entry);
    if (file_offset < 0) {
        P_ERRNO = P_ENOENT;
        return -1;
    }

    // mark the directory entry as deleted (set first byte to 1)
    entry.name[0] = 1;

    // write the modified directory entry back
    if (lseek(fs_fd, file_offset, SEEK_SET) == -1) {
        P_ERRNO = P_ELSEEK;
        return -1;
    }
    if (write(fs_fd, &entry, sizeof(entry)) != sizeof(entry)) {
        P_ERRNO = P_EWRITE;
        return -1;
    }

    // free all blocks in the file chain
    uint16_t current_block = entry.firstBlock;
    uint16_t next_block;

    while (current_block != FAT_FREE && current_block != FAT_EOF) {
        next_block = fat[current_block];
        fat[current_block] = FAT_FREE;
        current_block = next_block;
    }

    return 0;
}

/**
* Kernel-level call to re-position a file offset.
*/
int k_lseek(int fd, int offset, int whence) {
    // standard file descriptors don't support lseek
    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    
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


// helper function to format file information into a string
void format_file_info(dir_entry_t* entry, char* buffer) {
    // convert permissions to string
    char perms[4] = "---";
    if (entry->perm & PERM_READ) perms[0] = 'r';
    if (entry->perm & PERM_WRITE) perms[1] = 'w';
    if (entry->perm & PERM_READ_EXEC & ~PERM_READ) perms[2] = 'x';

    // convert time to string
    char time_str[20];
    struct tm* tm = localtime(&entry->mtime);
    strftime(time_str, sizeof(time_str), "%b %d %H:%M", tm);

    // format the output string
    snprintf(buffer, 256, "%4d %s %6d %s %s\n",
             entry->firstBlock,
             perms,
             entry->size,
             time_str,
             entry->name);
}

/**
* Kernel-level call to list files.
*/
int k_ls(const char* filename) {
    if (!is_mounted) {
        P_ERRNO = P_EFS_NOT_MOUNTED;
        return -1;
    }

    // start with root directory block  
    uint16_t current_block = 1;
    dir_entry_t entry;
    uint32_t offset_in_block = 0;
    
    // if filename is null, list all files in the current directory
    if (filename == NULL) {
        while (current_block != FAT_EOF) {
            // calculate absolute offset in filesystem
            off_t abs_offset = fat_size + (current_block - 1) * block_size + offset_in_block;
            
            // read directory entry
            if (lseek(fs_fd, abs_offset, SEEK_SET) == -1) {
                P_ERRNO = P_ELSEEK;
                return -1;
            }
            if (read(fs_fd, &entry, sizeof(entry)) != sizeof(entry)) {
                P_ERRNO = P_EREAD;
                return -1;
            }

            // check for end of directory
            if (entry.name[0] == 0) break;

            // skip deleted entries
            if (entry.name[0] == 1 || entry.name[0] == 2) {
                offset_in_block += sizeof(entry);
                // check if we need to move to next block
                if (offset_in_block + sizeof(entry) > block_size) {
                    current_block = fat[current_block];
                    offset_in_block = 0;
                }
                continue;
            }

            // format and write entry information
            char output_buffer[256];
            format_file_info(&entry, output_buffer);
            if (k_write(STDOUT_FILENO, output_buffer, strlen(output_buffer)) < 0) {
                return -1;
            }

            // move to next entry
            offset_in_block += sizeof(entry);
            // check if we need to move to next block
            if (offset_in_block + sizeof(entry) > block_size) {
                current_block = fat[current_block];
                offset_in_block = 0;
            }
        }
    } else {
        // find and display specific file
        int file_offset = find_file(filename, &entry);
        if (file_offset < 0) {
            P_ERRNO = P_ENOENT;
            return -1;
        }

        char output_buffer[256];
        format_file_info(&entry, output_buffer);
        if (k_write(STDOUT_FILENO, output_buffer, strlen(output_buffer)) < 0) {
            return -1;
        }
    }

    return 0;
}
