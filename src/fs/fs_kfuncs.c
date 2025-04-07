#include "fs_kfuncs.h"
#include "fat.h"
#include "../lib/pennos-errno.h"
#include "../kernel/kern-pcb.h"
#include <string.h>
#include <stdlib.h>
#include <time.h> // verify if needed
#include <stdio.h>
#include "fs_helpers.h"

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
static sys_fd_entry_t sys_fd_table[MAX_GLOBAL_FD];

// Global file system metadata structure
typedef struct {
    char filename[32];      // Filename (max 31 chars + null terminator)
    uint16_t first_block;   // First block of the file
    uint32_t size;          // Size of the file in bytes
    uint8_t permissions;    // Read (1), Write (2), Execute (4) permissions
    time_t timestamp;       // Last modification time
} file_metadata_t;

int k_open(const char *fname, int mode) {
    // validate inputs
    if (fname == NULL) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    if (mode == 0 || (mode & ~(F_READ | F_WRITE | F_APPEND))) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    
    int fd = find_free_fd();
    if (fd < 0) {
        P_ERRNO = P_EFULL;
        return -1;
    }
    
    int meta_idx = find_file_metadata(fname);
    
    // file doesn't exist
    if (meta_idx < 0) {
        if (!(mode & F_WRITE)) {
            P_ERRNO = P_ENOENT;
            return -1;
        }
        meta_idx = allocate_new_file(fname, 0x03);
        if (meta_idx < 0) {
            P_ERRNO = P_EFULL;
            return -1;
        }
    } else {
        file_metadata_t metadata;
        if (read_file_metadata(meta_idx, &metadata) < 0) {
            P_ERRNO = P_EUNKNOWN;
            return -1;
        }
        if ((mode & F_READ) && !(metadata.permissions & 0x01)) {
            P_ERRNO = P_EPERM;
            return -1;
        }
        if ((mode & (F_WRITE | F_APPEND)) && !(metadata.permissions & 0x02)) {
            P_ERRNO = P_EPERM;
            return -1;
        }
    }
    sys_fd_table[fd].in_use = true;
    sys_fd_table[fd].flags = mode;
    sys_fd_table[fd].ref_count = 1;
    sys_fd_table[fd].meta_idx = meta_idx;
    
    // set initial position
    if (mode & F_APPEND) {
        file_metadata_t metadata;
        if (read_file_metadata(meta_idx, &metadata) >= 0) {
            sys_fd_table[fd].position = metadata.size;
            
            if (metadata.size > 0 && metadata.first_block != FAT_FREE) {
                uint32_t block_offset;
                sys_fd_table[fd].curr_block = follow_fat_chain(metadata.first_block, metadata.size - 1, &block_offset);
                sys_fd_table[fd].block_offset = block_offset + 1;
            } else {
                sys_fd_table[fd].curr_block = FAT_FREE;
                sys_fd_table[fd].block_offset = 0;
            }
        } else {
            sys_fd_table[fd].position = 0;
            sys_fd_table[fd].curr_block = FAT_FREE;
            sys_fd_table[fd].block_offset = 0;
        }
    } else {
        sys_fd_table[fd].position = 0;
        file_metadata_t metadata;
        if (read_file_metadata(meta_idx, &metadata) >= 0 && 
            metadata.first_block != FAT_FREE) {
            sys_fd_table[fd].curr_block = metadata.first_block;
        } else {
            sys_fd_table[fd].curr_block = FAT_FREE;
        }
        sys_fd_table[fd].block_offset = 0;
    }
    
    return fd;
}

int k_read(int fd, int n, char *buf) {
    if (fd < 0 || fd >= MAX_GLOBAL_FD || !sys_fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }

    if (buf == NULL || n < 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }

    if (!(sys_fd_table[fd].flags & F_READ)) {
        P_ERRNO = P_EPERM;
        return -1;
    }

    int meta_idx = sys_fd_table[fd].meta_idx;
    file_metadata_t metadata;
    if (read_file_metadata(meta_idx, &metadata) < 0) {
        P_ERRNO = P_EUNKNOWN;
        return -1;
    }

    if (sys_fd_table[fd].position >= metadata.size) {
        return 0;
    }

    int bytes_to_read = n;
    if (sys_fd_table[fd].position + bytes_to_read > metadata.size) {
        bytes_to_read = metadata.size - sys_fd_table[fd].position;
    }
    
    uint16_t curr_block = sys_fd_table[fd].curr_block;
    uint32_t block_offset = sys_fd_table[fd].block_offset;
    
    // if no current block, start from first block
    if (curr_block == FAT_FREE) {
        curr_block = metadata.first_block;
        block_offset = 0;
    }
    
    int block_size = fat_get_block_size();
    int bytes_read = 0;
    
    while (bytes_read < bytes_to_read && curr_block != FAT_EOF && curr_block != FAT_FREE) {
        void* block_data = get_block_data(curr_block);
        if (!block_data) {
            P_ERRNO = P_EUNKNOWN;
            break;
        }
        
        int block_bytes_to_read = bytes_to_read - bytes_read;
        if (block_offset + block_bytes_to_read > block_size) {
            block_bytes_to_read = block_size - block_offset;
        }
        
        memcpy(buf + bytes_read, (char*)block_data + block_offset, block_bytes_to_read);
        bytes_read += block_bytes_to_read;
        
        if (block_offset + block_bytes_to_read >= block_size) {
            curr_block = fat_get_block(curr_block);
            block_offset = 0;
        } else {
            block_offset += block_bytes_to_read;
        }
    }
    
    sys_fd_table[fd].position += bytes_read;
    sys_fd_table[fd].curr_block = curr_block;
    sys_fd_table[fd].block_offset = block_offset;
    return bytes_read;
}

int k_write(int fd, const char *str, int n) {
    if (fd < 0 || fd >= MAX_GLOBAL_FD || !sys_fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }
    
    if (str == NULL || n < 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    
    if (!(sys_fd_table[fd].flags & (F_WRITE | F_APPEND))) {
        P_ERRNO = P_EPERM;
        return -1;
    }
    
    int meta_idx = sys_fd_table[fd].meta_idx;
    file_metadata_t metadata;
    if (read_file_metadata(meta_idx, &metadata) < 0) {
        P_ERRNO = P_EUNKNOWN;
        return -1;
    }
    
    uint16_t curr_block = sys_fd_table[fd].curr_block;
    uint32_t block_offset = sys_fd_table[fd].block_offset;
    uint32_t position = sys_fd_table[fd].position;
    
    if (curr_block == FAT_FREE && metadata.first_block != FAT_FREE && position > 0) {
        curr_block = follow_fat_chain(metadata.first_block, position, &block_offset);
    }

    if (curr_block == FAT_FREE) {
        curr_block = fat_find_free_block();
        if (curr_block < 0) {
            P_ERRNO = P_EFULL;
            return -1;
        }
        
        metadata.first_block = curr_block;
        fat_set_block(curr_block, FAT_EOF);
        block_offset = 0;
    }
    
    int block_size = fat_get_block_size();
    int bytes_written = 0;
    
    while (bytes_written < n) {
        void* block_data = get_block_data(curr_block);
        if (!block_data) {
            P_ERRNO = P_EUNKNOWN;
            break;
        }
        
        int block_bytes_to_write = n - bytes_written;
        if (block_offset + block_bytes_to_write > block_size) {
            block_bytes_to_write = block_size - block_offset;
        }
        
        memcpy((char*)block_data + block_offset, str + bytes_written, block_bytes_to_write);
        bytes_written += block_bytes_to_write;
        if (block_offset + block_bytes_to_write >= block_size) {
            uint16_t next_block = fat_get_block(curr_block);
            if (next_block == FAT_EOF || next_block == FAT_FREE) {
                next_block = fat_find_free_block();
                if (next_block < 0) {
                    break;
                }
                fat_set_block(curr_block, next_block);
                fat_set_block(next_block, FAT_EOF);
            }
            curr_block = next_block;
            block_offset = 0;
        } else {
            block_offset += block_bytes_to_write;
        }
    }
    sys_fd_table[fd].position += bytes_written;
    sys_fd_table[fd].curr_block = curr_block;
    sys_fd_table[fd].block_offset = block_offset;
    if (sys_fd_table[fd].position > metadata.size) {
        metadata.size = sys_fd_table[fd].position;
        metadata.timestamp = time(NULL);
        update_file_metadata(meta_idx, &metadata);
    }
    fat_save();
    return bytes_written;
}

int k_close(int fd) {
    // validate inputs
    if (fd < 0 || fd >= MAX_GLOBAL_FD || !sys_fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }
    
    // TODO
    return 0;
}

int k_unlink(const char *fname) {
    // validate inputs
    if (fname == NULL) {
        P_ERRNO = P_EINVAL;
        return -1;
    }

    // TODO
    return 0;
}

int k_lseek(int fd, int offset, int whence) {
    // validate inputs
    if (fd < 0 || fd >= MAX_GLOBAL_FD || !sys_fd_table[fd].in_use) {
        P_ERRNO = P_EBADF;
        return -1;
    }
    if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    
    int meta_idx = sys_fd_table[fd].meta_idx;
    file_metadata_t metadata;
    if (read_file_metadata(meta_idx, &metadata) < 0) {
        P_ERRNO = P_EUNKNOWN;
        return -1;
    }

    int new_position;
    switch (whence) {
        case SEEK_SET:
            new_position = offset;
            break;
        case SEEK_CUR:
            new_position = sys_fd_table[fd].position + offset;
            break;
        case SEEK_END:
            new_position = metadata.size + offset;
            break;
        default:
            P_ERRNO = P_EINVAL;
            return -1;
    }
    if (new_position < 0) {
        P_ERRNO = P_EINVAL;
        return -1;
    }
    
    sys_fd_table[fd].position = new_position;
    if (new_position == 0) {
        sys_fd_table[fd].curr_block = metadata.first_block;
        sys_fd_table[fd].block_offset = 0;
    } else if (new_position >= metadata.size) {
        uint32_t block_offset;
        if (metadata.size > 0 && metadata.first_block != FAT_FREE) {
            sys_fd_table[fd].curr_block = follow_fat_chain(metadata.first_block, metadata.size - 1, &block_offset);
            sys_fd_table[fd].block_offset = block_offset + 1;
            int block_size = fat_get_block_size();
            if (sys_fd_table[fd].block_offset >= block_size) {
                sys_fd_table[fd].curr_block = fat_get_block(sys_fd_table[fd].curr_block);
                sys_fd_table[fd].block_offset = 0;
            }
        } else {
            sys_fd_table[fd].curr_block = FAT_FREE;
            sys_fd_table[fd].block_offset = 0;
        }
    } else {
        uint32_t block_offset;
        sys_fd_table[fd].curr_block = follow_fat_chain(metadata.first_block, new_position, &block_offset);
        sys_fd_table[fd].block_offset = block_offset;
    }
    return new_position;
}

int k_ls(const char *filename) {
    // TODO
    return count;
}