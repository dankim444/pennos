#include "fs_helpers.h"
#include "fat_routines.h"
#include "lib/pennos-errno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>
#include <sys/mman.h>

////////////////////////////////////////////////////////////////////////////////
//                                 HELPERS                                    //
////////////////////////////////////////////////////////////////////////////////

int fs_fd = -1;
int block_size = 0;
int num_fat_blocks = 0;
int fat_size = 0;
uint16_t *fat = NULL;
bool is_mounted = false;
int MAX_FDS = 16;

// helper for initializing the fd table
void init_fd_table(fd_entry_t *fd_table) {
    for (int i = 0; i < MAX_FDS; i++) {
        fd_table[i].in_use = 0;
    }
}

// helper to get a free file descriptor
int get_free_fd(fd_entry_t *fd_table) {
    for (int i = 0; i < MAX_FDS; i++) {
        if (!fd_table[i].in_use) {
            return i;
        }
    }
    return -1;
}

// helper function to allocate a block
uint16_t allocate_block() {
    // start from block 2 (blocks 0 and 1 are reserved)
    for (int i = 2; i < fat_size / 2; i++) {
        if (fat[i] == FAT_FREE) {
            fat[i] = FAT_EOF;
            return i;
        }
    }
    return 0; // indicates error in uint16_t
}

// helper function to find a file in the root directory
int find_file(const char *filename, dir_entry_t *entry) {
    if (!is_mounted) {
        P_ERRNO = P_FS_NOT_MOUNTED;
        return -1;
    }
    
    // read the root directory block (always block 1)
    if (lseek(fs_fd, fat_size, SEEK_SET) == -1) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    
    // read directory entries until we find a match or end of directory
    dir_entry_t dir_entry;
    int offset = 0;
    
    while (offset < block_size) {
        if (read(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
            P_ERRNO = P_EINVAL;
            return -1;
        }
        
        // check if we've reached the end of directory
        if (dir_entry.name[0] == 0) {
            break;
        }
        
        // check if this is a deleted entry
        if (dir_entry.name[0] == 1 || dir_entry.name[0] == 2) {
            offset += sizeof(dir_entry);
            continue;
        }
        
        // check if we found the file
        if (strcmp(dir_entry.name, filename) == 0) {
            if (entry) {
                memcpy(entry, &dir_entry, sizeof(dir_entry));
            }
            return offset;
        }
        
        offset += sizeof(dir_entry);
    }
    
    // file not found
    P_ERRNO = P_ENOENT;
    return -1;
}

// helper function to add a file to the root directory
int add_file_entry(const char *filename, uint32_t size, uint16_t first_block, uint8_t type, uint8_t perm) {
    if (!is_mounted) {
        P_ERRNO = P_FS_NOT_MOUNTED;
        return -1;
    }
    
    // check if file already exists
    dir_entry_t existing;
    if (find_file(filename, &existing) >= 0) {
        P_ERRNO = P_EEXIST;
        return -1;
    }
    
    // read the root directory block (always block 1)
    if (lseek(fs_fd, fat_size, SEEK_SET) == -1) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    
    // look for a free slot (deleted entry or end of directory)
    dir_entry_t dir_entry;
    int offset = 0;
    
    while (offset < block_size) {
        if (read(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
            P_ERRNO = P_EINVAL;
            return -1;
        }
        
        // check if this slot is free
        if (dir_entry.name[0] == 0 || dir_entry.name[0] == 1) {
            memset(&dir_entry, 0, sizeof(dir_entry));
            strncpy(dir_entry.name, filename, 31);
            dir_entry.size = size;
            dir_entry.firstBlock = first_block;
            dir_entry.type = type;
            dir_entry.perm = perm;
            dir_entry.mtime = time(NULL); // TODO: check if this is how this is done
            
            // write the entry back
            if (lseek(fs_fd, fat_size + offset, SEEK_SET) == -1 ||
                write(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
                P_ERRNO = P_EINVAL;
                return -1;
            }
            
            return offset;
        }
        
        offset += sizeof(dir_entry);
    }
    
    // no free slots found
    P_ERRNO = P_EFULL;
    return -1;
}

// helper function to copy data from host OS to PennFAT
int copy_host_to_pennfat(const char *host_filename, const char *pennfat_filename) {
    if (!is_mounted) {
        P_ERRNO = P_FS_NOT_MOUNTED;
        return -1;
    }
    
    // open the host file
    int host_fd = open(host_filename, O_RDONLY);
    if (host_fd == -1) {
        P_ERRNO = P_ENOENT;
        return -1;
    }
    
    // determine file size by seeking to the end and getting position
    off_t file_size = lseek(host_fd, 0, SEEK_END); 
    if (file_size == -1) {
        P_ERRNO = P_EINVAL;
        close(host_fd);
        return -1;
    }

    // go back to beginning of file for reading
    if (lseek(host_fd, 0, SEEK_SET) == -1) {
        P_ERRNO = P_EINVAL;
        close(host_fd);
        return -1;
    }
    
    // allocate the first block
    uint16_t first_block = allocate_block();
    if (first_block == 0) {
        P_ERRNO = P_EFULL;
        close(host_fd);
        return -1;
    }
    
    // create the file entry in the directory
    if (add_file_entry(pennfat_filename, file_size, first_block, TYPE_REGULAR, PERM_READ_WRITE) == -1) {
        // TODO: deallocate the block if failed
        close(host_fd);
        return -1;
    }
    
    // copy the data into this buffer
    uint8_t *buffer = (uint8_t *)malloc(block_size);
    if (!buffer) {
        P_ERRNO = P_EINVAL;
        close(host_fd);
        return -1;
    }
    
    uint16_t current_block = first_block;
    uint32_t bytes_remaining = file_size;
    
    while (bytes_remaining > 0) {
        // read from host file
        ssize_t bytes_to_read = bytes_remaining < block_size ? bytes_remaining : block_size;
        ssize_t bytes_read = read(host_fd, buffer, bytes_to_read);
        
        if (bytes_read <= 0) {
            break; // reached end of file or error
        }
        
        // write to PennFAT
        if (lseek(fs_fd, fat_size + (current_block - 1) * block_size, SEEK_SET) == -1 || 
            write(fs_fd, buffer, bytes_read) != bytes_read) {
            P_ERRNO = P_EINVAL;
            free(buffer);
            close(host_fd);
            return -1;
        }
        
        bytes_remaining -= bytes_read;
        
        // if more data to write, allocate a new block
        if (bytes_remaining > 0) {
            uint16_t next_block = allocate_block();
            if (next_block == 0) {
                P_ERRNO = P_EFULL;
                free(buffer);
                close(host_fd);
                return -1;
            }
            
            // update the FAT chain
            fat[current_block] = next_block;
            current_block = next_block;
        }
    }
    
    free(buffer);
    close(host_fd);
    return 0;
}

int copy_pennfat_to_host(const char *pennfat_filename, const char *host_filename) {
    return 0;
}