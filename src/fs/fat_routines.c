#include "fat_routines.h"
#include "fs_kfuncs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

// Global variables to track the current mounted filesystem
//static int fs_fd = -1; // fd for the mounted filesystem
//static uint16_t *fat = NULL; // pointer to the FAT in memory
static int block_size = 0; // block size of the current filesystem
static int fat_size = 0; // size of the FAT in bytes

/**
* Creates a PennFAT filesystem in the file named fs_name
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
    block_size = actual_block_size; // set the global variable
    
    // compute size (for both fat and all of file system)
    fat_size = num_blocks * actual_block_size;
    int num_entries = num_blocks * actual_block_size / 2; // assumes each entry is 2 bytes
    size_t filesystem_size = fat_size + (actual_block_size * (num_entries - 1));
    
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
    
    // initialize the FAT
    uint16_t *temp_fat = (uint16_t *)calloc(num_entries, sizeof(uint16_t));
    if (!temp_fat) {
        close(fd);
        return -1;
    }
    
    // configure first two entries (metadata and root directory)
    temp_fat[0] = (num_blocks << 8) | block_size;
    temp_fat[1] = 0xFFFF;
    
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

int mount(const char *fs_name) {
    return 0;
}

int unmount() {
    return 0;
}

void* cat(void *arg) {
    return 0;
}

void* ls(void *arg) {
    return 0;
}

void* touch(void *arg) {
    return 0;
}

void* mv(void *arg) {
    return 0;
}

void* cp(void *arg) {
    return 0;
}

void* rm(void *arg) {
    return 0;
}

// void* chmod(void *arg) {
//     return 0;
// }