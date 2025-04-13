// #include "fs_kfuncs.h"
// #include "../lib/pennos-errno.h"
// #include "../kernel/kern_pcb.h"
// #include "fs_helpers.h"
// #include "fs_syscalls.h"  // F_READ, F_WRITE, F_APPEND, STDIN_FILENO, STDOUT_FILENO, STDIN_FILENO, STDERR_FILENO
// #include <string.h>
// #include <stdlib.h>
// #include <time.h>
// #include <stdio.h>
// #include "fat_routines.h"

// // Global file descriptor table
// sys_fd_entry_t sys_fd_table[MAX_GLOBAL_FD];

// /**
//  * Kernel-level call to open a file.
//  */
// int k_open(const char *fname, int mode) {
//     // validate inputs
//     if (fname == NULL) {
//         P_ERRNO = P_EINVAL;
//         return -1;
//     }
//     if (mode == 0 || (mode & ~(F_READ | F_WRITE | F_APPEND))) {
//         P_ERRNO = P_EINVAL;
//         return -1;
//     }
    
//     int fd = find_free_fd();
//     if (fd < 0) {
//         P_ERRNO = P_EFULL;
//         return -1;
//     }
    
//     int meta_idx = find_file_metadata(fname);
    
//     // file doesn't exist
//     if (meta_idx < 0) {
//         if (!(mode & F_WRITE)) {
//             P_ERRNO = P_ENOENT;
//             return -1;
//         }
//         meta_idx = allocate_new_file(fname, 0x03);
//         if (meta_idx < 0) {
//             P_ERRNO = P_EFULL;
//             return -1;
//         }
//     } else {
//         file_metadata_t metadata;
//         if (read_file_metadata(meta_idx, &metadata) < 0) {
//             P_ERRNO = P_EUNKNOWN;
//             return -1;
//         }
//         if ((mode & F_READ) && !(metadata.permissions & 0x01)) {
//             P_ERRNO = P_EPERM;
//             return -1;
//         }
//         if ((mode & (F_WRITE | F_APPEND)) && !(metadata.permissions & 0x02)) {
//             P_ERRNO = P_EPERM;
//             return -1;
//         }
//     }
    
//     sys_fd_table[fd].in_use = 1;
//     sys_fd_table[fd].flags = mode;
//     sys_fd_table[fd].meta_idx = meta_idx;
    
//     // set initial position
//     if (mode & F_APPEND) {
//         file_metadata_t metadata;
//         if (read_file_metadata(meta_idx, &metadata) >= 0) {
//             sys_fd_table[fd].offset = metadata.size;
//         } else {
//             sys_fd_table[fd].offset = 0;
//         }
//     } else {
//         sys_fd_table[fd].offset = 0;
//     }
    
//     return fd;
// }

// /**
//  * Kernel-level call to read a file.
//  */
// int k_read(int fd, int n, char *buf) {
//     if (fd < 0 || fd >= MAX_GLOBAL_FD || !sys_fd_table[fd].in_use) {
//         P_ERRNO = P_EBADF;
//         return -1;
//     }

//     if (buf == NULL || n < 0) {
//         P_ERRNO = P_EINVAL;
//         return -1;
//     }

//     if (!(sys_fd_table[fd].flags & F_READ)) {
//         P_ERRNO = P_EPERM;
//         return -1;
//     }

//     int meta_idx = sys_fd_table[fd].meta_idx;
//     file_metadata_t metadata;
//     if (read_file_metadata(meta_idx, &metadata) < 0) {
//         P_ERRNO = P_EUNKNOWN;
//         return -1;
//     }

//     if (sys_fd_table[fd].offset >= metadata.size) {
//         return 0;
//     }

//     int bytes_to_read = n;
//     if (sys_fd_table[fd].offset + bytes_to_read > metadata.size) {
//         bytes_to_read = metadata.size - sys_fd_table[fd].offset;
//     }
    
//     uint16_t curr_block;
//     uint32_t block_offset;
    
//     curr_block = follow_fat_chain(metadata.first_block, sys_fd_table[fd].offset, &block_offset);
    
//     int block_size = fat_get_block_size();
//     int bytes_read = 0;
    
//     while (bytes_read < bytes_to_read && curr_block != FAT_EOF && curr_block != FAT_FREE) {
//         void* block_data = get_block_data(curr_block);
//         if (!block_data) {
//             P_ERRNO = P_EUNKNOWN;
//             break;
//         }
        
//         int block_bytes_to_read = bytes_to_read - bytes_read;
//         if (block_offset + block_bytes_to_read > block_size) {
//             block_bytes_to_read = block_size - block_offset;
//         }
        
//         memcpy(buf + bytes_read, (char*)block_data + block_offset, block_bytes_to_read);
//         bytes_read += block_bytes_to_read;
        
//         if (block_offset + block_bytes_to_read >= block_size) {
//             curr_block = fat_get_block(curr_block);
//             block_offset = 0;
//         } else {
//             block_offset += block_bytes_to_read;
//         }
//     }
    
//     sys_fd_table[fd].offset += bytes_read;
//     return bytes_read;
// }

// /**
//  * Kernel-level call to write to a file.
//  */
// int k_write(int fd, const char *str, int n) {
//     if (fd < 0 || fd >= MAX_GLOBAL_FD || !sys_fd_table[fd].in_use) {
//         P_ERRNO = P_EBADF;
//         return -1;
//     }
    
//     if (str == NULL || n < 0) {
//         P_ERRNO = P_EINVAL;
//         return -1;
//     }
    
//     if (!(sys_fd_table[fd].flags & (F_WRITE | F_APPEND))) {
//         P_ERRNO = P_EPERM;
//         return -1;
//     }
    
//     int meta_idx = sys_fd_table[fd].meta_idx;
//     file_metadata_t metadata;
//     if (read_file_metadata(meta_idx, &metadata) < 0) {
//         P_ERRNO = P_EUNKNOWN;
//         return -1;
//     }
    
//     uint16_t curr_block;
//     uint32_t block_offset;
//     uint32_t position = sys_fd_table[fd].offset;
    
//     // If at EOF, we need to allocate a new block
//     if (position >= metadata.size) {
//         if (metadata.first_block == FAT_FREE || metadata.size == 0) {
//             // File is empty, need to allocate first block
//             curr_block = fat_find_free_block();
//             if (curr_block < 0) {
//                 P_ERRNO = P_EFULL;
//                 return -1;
//             }
            
//             metadata.first_block = curr_block;
//             fat_set_block(curr_block, FAT_EOF);
//             block_offset = 0;
//         } else {
//             // Follow the chain to the last block
//             uint32_t last_block_offset;
//             uint16_t last_block = follow_fat_chain(metadata.first_block, metadata.size - 1, &last_block_offset);
            
//             int block_size = fat_get_block_size();
//             if (last_block_offset + 1 >= block_size) {
//                 // Need a new block
//                 curr_block = fat_find_free_block();
//                 if (curr_block < 0) {
//                     P_ERRNO = P_EFULL;
//                     return -1;
//                 }
//                 fat_set_block(last_block, curr_block);
//                 fat_set_block(curr_block, FAT_EOF);
//                 block_offset = 0;
//             } else {
//                 // Continue in the last block
//                 curr_block = last_block;
//                 block_offset = last_block_offset + 1;
//             }
//         }
//     } else {
//         // Position is within existing file
//         curr_block = follow_fat_chain(metadata.first_block, position, &block_offset);
//     }
    
//     int block_size = fat_get_block_size();
//     int bytes_written = 0;
    
//     while (bytes_written < n) {
//         void* block_data = get_block_data(curr_block);
//         if (!block_data) {
//             P_ERRNO = P_EUNKNOWN;
//             break;
//         }
        
//         int block_bytes_to_write = n - bytes_written;
//         if (block_offset + block_bytes_to_write > block_size) {
//             block_bytes_to_write = block_size - block_offset;
//         }
        
//         memcpy((char*)block_data + block_offset, str + bytes_written, block_bytes_to_write);
//         bytes_written += block_bytes_to_write;
        
//         if (block_offset + block_bytes_to_write >= block_size) {
//             uint16_t next_block = fat_get_block(curr_block);
//             if (next_block == FAT_EOF || next_block == FAT_FREE) {
//                 next_block = fat_find_free_block();
//                 if (next_block < 0) {
//                     break;
//                 }
//                 fat_set_block(curr_block, next_block);
//                 fat_set_block(next_block, FAT_EOF);
//             }
//             curr_block = next_block;
//             block_offset = 0;
//         } else {
//             block_offset += block_bytes_to_write;
//         }
//     }
    
//     sys_fd_table[fd].offset += bytes_written;
    
//     if (sys_fd_table[fd].offset > metadata.size) {
//         metadata.size = sys_fd_table[fd].offset;
//         metadata.timestamp = time(NULL);
//         update_file_metadata(meta_idx, &metadata);
//     }
    
//     // call a function called fat_sync here
//     return bytes_written;
// }

// /**
//  * Kernel-level call to close a file.
//  */
// int k_close(int fd) {
//     // validate inputs
//     if (fd < 0 || fd >= MAX_GLOBAL_FD || !sys_fd_table[fd].in_use) {
//         P_ERRNO = P_EBADF;
//         return -1;
//     }
    
//     // Release resources
//     sys_fd_table[fd].in_use = 0;
//     sys_fd_table[fd].flags = 0;
//     sys_fd_table[fd].meta_idx = 0;
//     sys_fd_table[fd].offset = 0;
    
//     return 0;
// }

// /**
//  * Kernel-level call to remove a file.
//  */
// int k_unlink(const char *fname) {
//     // validate inputs
//     if (fname == NULL) {
//         P_ERRNO = P_EINVAL;
//         return -1;
//     }

//     int meta_idx = find_file_metadata(fname);
//     if (meta_idx < 0) {
//         P_ERRNO = P_ENOENT;
//         return -1;
//     }
    
//     file_metadata_t metadata;
//     if (read_file_metadata(meta_idx, &metadata) < 0) {
//         P_ERRNO = P_EUNKNOWN;
//         return -1;
//     }
    
//     // Check if the file is currently open by any process
//     for (int i = 0; i < MAX_GLOBAL_FD; i++) {
//         if (sys_fd_table[i].in_use && sys_fd_table[i].meta_idx == meta_idx) {
//             P_ERRNO = P_EBUSY;
//             return -1;
//         }
//     }
    
//     // Free all blocks in the file
//     uint16_t curr_block = metadata.first_block;
//     while (curr_block != FAT_EOF && curr_block != FAT_FREE) {
//         uint16_t next_block = fat_get_block(curr_block);
//         fat_set_block(curr_block, FAT_FREE);
//         curr_block = next_block;
//     }
    
//     // Mark metadata entry as unused
//     metadata.is_used = 0;
//     update_file_metadata(meta_idx, &metadata);
    
//     // call fat_sync here
//     return 0;
// }

// /**
//  * Kernel-level call to re-position a file offset.
//  */
// int k_lseek(int fd, int offset, int whence) {
//     // validate inputs
//     if (fd < 0 || fd >= MAX_GLOBAL_FD || !sys_fd_table[fd].in_use) {
//         P_ERRNO = P_EBADF;
//         return -1;
//     }
//     if (whence != SEEK_SET && whence != SEEK_CUR && whence != SEEK_END) {
//         P_ERRNO = P_EINVAL;
//         return -1;
//     }
    
//     int meta_idx = sys_fd_table[fd].meta_idx;
//     file_metadata_t metadata;
//     if (read_file_metadata(meta_idx, &metadata) < 0) {
//         P_ERRNO = P_EUNKNOWN;
//         return -1;
//     }

//     int new_position;
//     switch (whence) {
//         case SEEK_SET:
//             new_position = offset;
//             break;
//         case SEEK_CUR:
//             new_position = sys_fd_table[fd].offset + offset;
//             break;
//         case SEEK_END:
//             new_position = metadata.size + offset;
//             break;
//         default:
//             P_ERRNO = P_EINVAL;
//             return -1;
//     }
    
//     if (new_position < 0) {
//         P_ERRNO = P_EINVAL;
//         return -1;
//     }
    
//     sys_fd_table[fd].offset = new_position;
//     return new_position;
// }

// /**
//  * Kernel-level call to list files.
//  */
// int k_ls(const char *filename) {
//     int count = 0;
//     int total_entries = get_metadata_count();
    
//     for (int i = 0; i < total_entries; i++) {
//         file_metadata_t metadata;
//         if (read_file_metadata(i, &metadata) == 0 && metadata.is_used) {
//             if (filename == NULL || strcmp(metadata.filename, filename) == 0) {
//                 // Format: [index] [permissions] [size] [timestamp] [filename]
//                 printf("%d %c%c%c %u %u %s\n", 
//                        i + 2, // Display as FD (offset by 2)
//                        (metadata.permissions & 0x04) ? 'x' : '-',
//                        (metadata.permissions & 0x02) ? 'w' : '-',
//                        (metadata.permissions & 0x01) ? 'r' : '-',
//                        metadata.size,
//                        (unsigned int)metadata.timestamp,
//                        metadata.filename);
//                 count++;
                
//                 // If looking for a specific file and found it, stop
//                 if (filename != NULL) {
//                     break;
//                 }
//             }
//         }
//     }
    
//     return count;
// }