#include "fs_kfuncs.h"
#include "fat.h"
#include "../lib/pennos-errno.h"
#include "../kernel/kern-pcb.h"
#include <string.h>
#include <stdlib.h>
#include <time.h> // verify if needed

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

// this struct is an entry in the global file descriptor table
typedef struct {
    bool in_use;
    int flags;              // mode (F_READ, F_WRITE, F_APPEND)
    int ref_count;          // tracks how many processes reference this file
    uint32_t position;      // position = (block_index × block_size) + block_offset
    int meta_idx;           // stores the index in the file system's metadata array 
                            // where information about this file is stored (ie. name, size, firstBlock, etc.)
    uint16_t curr_block;    // current block number corresponding to current file position
    uint32_t block_offset;  // offset within current block
} sys_fd_entry_t;

// Global file descriptor table
#define MAX_GLOBAL_FD 128 // TODO: check if this is the right size
static sys_fd_entry_t sys_fd_table[MAX_GLOBAL_FD]; // TODO: check, do we want to make this static?

int k_open(const char *fname, int mode) {
    return -1; // TODO --> implement k_open
}

int k_read(int fd, int n, char *buf) {
    return -1; // TODO --> implement k_read
}

int k_write(int fd, const char *str, int n) {
    return -1; // TODO --> implement k_write
}

int k_close(int fd) {
    return -1; // TODO --> implement k_close
}

int k_unlink(const char *fname) {
    return -1; // TODO --> implement k_unlink
}

int k_lseek(int fd, int offset, int whence) {
    return -1; // TODO --> implement k_lseek
}

int k_ls(const char *filename) {
    return -1; // TODO --> implement k_ls
}