#include "fat_routines.h"
#include "fs_kfuncs.h"
#include "fs_helpers.h"

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
//                                 GLOBALS                                    //
////////////////////////////////////////////////////////////////////////////////

// Global variables to track the current mounted filesystem
extern int fs_fd;
extern int block_size;
extern int num_fat_blocks;
extern int fat_size;
extern uint16_t *fat;
extern bool is_mounted;
extern int MAX_FDS; // TODO: Figure out if we want more file descriptors
static fd_entry_t fd_table[16]; // file decriptor table


////////////////////////////////////////////////////////////////////////////////
//                                 ROUTINES                                   //
////////////////////////////////////////////////////////////////////////////////

/**
* Creates a PennFAT filesystem in the file named fs_name at the OS-level
*/
int mkfs(const char *fs_name, int num_blocks, int blk_size) {
    // validate arguments
    if (num_blocks < 1 || num_blocks > 32) {
        return -1;
    }
    if (blk_size < 0 || blk_size > 4) {
        return -1;
    }
    
    // determine actual block size
    int block_sizes[] = {256, 512, 1024, 2048, 4096};
    int actual_block_size = block_sizes[blk_size];
    
    // compute size (for both fat and all of file system)
    int fat_size = num_blocks * actual_block_size;
    int fat_entries = fat_size / 2;
    int num_data_blocks = (num_blocks == 32) ? fat_entries - 2 : fat_entries - 1;
    size_t filesystem_size = fat_size + (actual_block_size * num_data_blocks);
    
    // create the file for the filesystem
    int fd = open(fs_name, O_RDWR | O_CREAT | O_TRUNC);
    if (fd == -1) {
        return -1;
    }
    
    // extend the file to the required size
    if (ftruncate(fd, filesystem_size) == -1) {
        close(fd);
        return -1;
    }
    
    // allocate the FAT
    uint16_t *temp_fat = (uint16_t *)calloc(fat_entries, sizeof(uint16_t));
    if (!temp_fat) {
        close(fd);
        return -1;
    }
    
    // initialize the first two entries of FAT (metadata and root directory)
    temp_fat[0] = (num_blocks << 8) | block_size;
    temp_fat[1] = FAT_EOF;
    
    // write the FAT to the file
    if (write(fd, temp_fat, fat_size) != fat_size) {
        free(temp_fat);
        close(fd);
        return -1;
    }
    
    // initialize the root directory with an empty entry
    uint8_t *root_dir = (uint8_t *)calloc(actual_block_size, 1);
    // write the root directory block
    if (lseek(fd, fat_size, SEEK_SET) == -1 || write(fd, root_dir, actual_block_size) != actual_block_size) {
        free(temp_fat);
        free(root_dir);
        close(fd);
        return -1;
    }
    
    // clean up
    free(temp_fat);
    free(root_dir);
    close(fd);
    return 0;
}

/**
* Mounts a filesystem with name fs_name by loading its FAT into memory.
*/
int mount(const char *fs_name) {
    // check if a filesystem is already mounted
    if (is_mounted) {
        // set error --> P_ERRNO = P_EBUSY;
        return -1; // another fs has already been mounted
    }

    // open the file with fs_name + set the global fs_fd
    fs_fd = open(fs_name, O_RDWR);
    if (fs_fd == -1) {
        // set error --> P_ERRNO = P_ENOENT;
        return -1;
    }

    // read the first two bytes to get configuration
    uint16_t config;
    if (read(fs_fd, &config, sizeof(config)) != sizeof(config)) {
        // set error --> P_ERRNO = P_EINVAL;
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    // extract size information from first two bytes
    num_fat_blocks = (config >> 8) & 0xFF;
    block_size = config & 0xFF;
    fat_size = num_fat_blocks * block_size;

    // map the FAT into memory
    if (lseek(fs_fd, 0, SEEK_SET) == -1) {
        // set error -> P_ERRNO = P_EINVAL;
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    fat = mmap(NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
    if (fat == MAP_FAILED) {
        // set error --> P_ERRNO = P_EINVAL;
        close(fs_fd);
        fs_fd = -1;
        return -1;
    }

    // initialize the fd table
    init_fd_table(fd_table);
    is_mounted = true;
    return 0;
}

int unmount() {
    // first check that a file system is actually mounted
    if (!is_mounted) {
        // set error code --> P_ERRNO = P_FS_NOT_MOUNTED;
        return -1;
    }

    // unmap the FAT
    if (fat != NULL) {
        if (munmap(fat, fat_size) == -1) {
            // set error code --> P_ERRNO = P_EINVAL;
            return -1;
        }
        fat = NULL;
    }

    // close fs_fd
    if (fs_fd != -1) {
        if (close(fs_fd) == -1) {
            // set error code --> P_ERRNO = P_EBADF;
            return -1;
        }
        fs_fd = -1;
    }

    // reset the other globals
    num_fat_blocks = 0;
    block_size = 0;
    fat_size = 0;
    is_mounted = false;
    return 0;
}

void* cat(void *arg) {
    return 0;
}

/**
* List all files in the directory.
*/
void* ls(void *arg) {
    return 0;
}

void* touch(void *arg) {
    return 0;
}

void* mv(void *arg) {
    return 0;
}

/**
* Copies the source file to the destination.
*/
void* cp(void *arg) {
    return 0;
}

void* rm(void *arg) {
    return 0;
}

// void* chmod(void *arg) {
//     return 0;
// }