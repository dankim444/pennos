#include "fs_helpers.h"
#include "fat_routines.h"
#include "fs_kfuncs.h"
#include "lib/pennos-errno.h"
#include "shell/builtins.h"

#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////
//                                 HELPERS                                    //
////////////////////////////////////////////////////////////////////////////////

int fs_fd = -1;
int block_size = 0;
int num_fat_blocks = 0;
int fat_size = 0;
uint16_t* fat = NULL;
bool is_mounted = false;
int MAX_FDS = 16;
fd_entry_t fd_table[1024];

// helper for initializing the fd table
void init_fd_table(fd_entry_t* fd_table) {
  for (int i = 0; i < MAX_FDS; i++) {
    fd_table[i].in_use = 0;
  }
}

// helper to get a free file descriptor
int get_free_fd(fd_entry_t* fd_table) {
  for (int i = 0; i < MAX_FDS; i++) {
    if (!fd_table[i].in_use) {
      return i;
    }
  }
  return -1;
}

// void print_fat_state(const char* label) {
//   printf("\n--- FAT STATE: %s ---\n", label);

//   printf("First 10 entries:\n");
//   for (int i = 0; i < 10 && i < fat_size / 2; i++) {
//     printf("FAT[%d] = 0x%04x\n", i, fat[i]);
//   }

//   int free_count = 0;
//   for (int i = 2; i < fat_size / 2; i++) {
//     if (fat[i] == FAT_FREE)
//       free_count++;
//   }

//   printf("Total free blocks: %d\n", free_count);
//   printf("Total usable blocks: %d\n", fat_size / 2 - 2);
//   printf("------------------------\n\n");
// }

// helper function to allocate a block
uint16_t allocate_block() {
  // print_fat_state("Before allocating block");
  //  start from block 2 (blocks 0 and 1 are reserved)
  for (int i = 2; i < fat_size / 2; i++) {
    if (fat[i] == FAT_FREE) {
      fat[i] = FAT_EOF;
      // print_fat_state("After allocating block");
      return i;
    }
  }
  return 0;  // indicates error in uint16_t
}

// helper function to find a file in the root directory
int find_file(const char* filename, dir_entry_t* entry) {
  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    return -1;
  }

  // Start with root directory block (block 1)
  // TODO: REPLACE WITH K_LSEEK
  uint16_t current_block = 1;
  int offset = 0;
  dir_entry_t dir_entry;

  while (1) {
    // Position at the start of current block
    if (lseek(fs_fd, fat_size + (current_block - 1) * block_size, SEEK_SET) ==
        -1) {
      P_ERRNO = P_EINVAL;
      return -1;
    }

    // Search current block
    offset = 0;
    while (offset < block_size) {
      // TODO: REPLACE WITH K_READ
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

    // If we've reached the end of the current block, check if there's a next
    // block
    if (fat[current_block] != FAT_EOF) {
      current_block = fat[current_block];
      continue;
    }

    // No more blocks to search
    break;
  }

  // file not found
  P_ERRNO = P_ENOENT;
  return -1;
}

// helper function to add a file to the root directory
int add_file_entry(const char* filename,
                   uint32_t size,
                   uint16_t first_block,
                   uint8_t type,
                   uint8_t perm) {
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

  // Start with root directory block (block 1)
  uint16_t current_block = 1;
  int offset = 0;
  dir_entry_t dir_entry;

  while (1) {
    // Position at the start of current block
    if (lseek(fs_fd, fat_size + (current_block - 1) * block_size, SEEK_SET) ==
        -1) {
      P_ERRNO = P_EINVAL;
      return -1;
    }

    // Reset offset for new block
    offset = 0;

    // Search current block for free slot
    while (offset < block_size) {
      if (read(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
        P_ERRNO = P_EINVAL;
        return -1;
      }

      // Found a free slot
      if (dir_entry.name[0] == 0 || dir_entry.name[0] == 1) {
        // Prepare new entry
        memset(&dir_entry, 0, sizeof(dir_entry));
        strncpy(dir_entry.name, filename, 31);
        dir_entry.size = size;
        dir_entry.firstBlock = first_block;
        dir_entry.type = type;
        dir_entry.perm = perm;
        dir_entry.mtime = time(NULL);

        // Write the entry
        if (lseek(fs_fd, fat_size + (current_block - 1) * block_size + offset,
                  SEEK_SET) == -1 ||
            write(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
          P_ERRNO = P_EINVAL;
          return -1;
        }

        return offset;
      }

      offset += sizeof(dir_entry);
    }

    // Current block is full, check if there's a next block
    if (fat[current_block] != FAT_EOF) {
      // Follow the chain to next block
      current_block = fat[current_block];
      continue;
    }

    // Need to allocate a new block
    uint16_t new_block = allocate_block();
    if (new_block == 0) {
      P_ERRNO = P_EFULL;
      return -1;
    }

    // Chain the new block
    fat[current_block] = new_block;
    fat[new_block] = FAT_EOF;

    // Initialize new block with zeros
    uint8_t* zero_block = calloc(block_size, 1);
    if (!zero_block) {
      P_ERRNO = P_EINVAL;
      return -1;
    }

    if (lseek(fs_fd, fat_size + (new_block - 1) * block_size, SEEK_SET) == -1 ||
        write(fs_fd, zero_block, block_size) != block_size) {
      free(zero_block);
      P_ERRNO = P_EINVAL;
      return -1;
    }

    free(zero_block);

    // Write the new entry at the start of the new block
    memset(&dir_entry, 0, sizeof(dir_entry));
    strncpy(dir_entry.name, filename, 31);
    dir_entry.size = size;
    dir_entry.firstBlock = first_block;
    dir_entry.type = type;
    dir_entry.perm = perm;
    dir_entry.mtime = time(NULL);

    if (lseek(fs_fd, fat_size + (new_block - 1) * block_size, SEEK_SET) == -1 ||
        write(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
      P_ERRNO = P_EINVAL;
      return -1;
    }

    return 0;
  }
}

// helper function to copy data from host OS to PennFAT
int copy_host_to_pennfat(const char* host_filename,
                         const char* pennfat_filename) {
  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    return -1;
  }

  // open the host file
  // TODO: REPLACE WITH K_OPEN
  int host_fd = open(host_filename, O_RDONLY);
  if (host_fd == -1) {
    P_ERRNO = P_ENOENT;
    return -1;
  }

  // determine file size by seeking to the end and getting position
  // TODO: REPLACE WITH K_LSEEK
  off_t file_size = lseek(host_fd, 0, SEEK_END);
  if (file_size == -1) {
    P_ERRNO = P_EINVAL;
    // TODO: REPLACE WITH K_CLOSE
    close(host_fd);
    return -1;
  }

  // go back to beginning of file for reading
  // TODO: REPLACE WITH K_LSEEK
  if (lseek(host_fd, 0, SEEK_SET) == -1) {
    P_ERRNO = P_EINVAL;
    // TODO: REPLACE WITH K_CLOSE
    close(host_fd);
    return -1;
  }

  // allocate the first block
  uint16_t first_block = allocate_block();
  if (first_block == 0) {
    P_ERRNO = P_EFULL;
    // TODO: REPLACE WITH K_CLOSE
    close(host_fd);
    return -1;
  }

  // create the file entry in the directory
  if (add_file_entry(pennfat_filename, file_size, first_block, TYPE_REGULAR,
                     PERM_READ_WRITE) == -1) {
    // TODO: deallocate the block if failed
    // TODO: REPLACE WITH K_CLOSE
    close(host_fd);
    return -1;
  }

  // copy the data into this buffer
  uint8_t* buffer = (uint8_t*)malloc(block_size);
  if (!buffer) {
    P_ERRNO = P_EINVAL;
    // TODO: REPLACE WITH K_CLOSE
    close(host_fd);
    return -1;
  }

  uint16_t current_block = first_block;
  uint32_t bytes_remaining = file_size;

  while (bytes_remaining > 0) {
    // read from host file
    ssize_t bytes_to_read =
        bytes_remaining < block_size ? bytes_remaining : block_size;
    // TODO: REPLACE WITH K_READ
    ssize_t bytes_read = read(host_fd, buffer, bytes_to_read);

    if (bytes_read <= 0) {
      break;  // reached end of file or error
    }

    // write to PennFAT
    // TODO: REPLACE WITH K_LSEEK
    if (lseek(fs_fd, fat_size + (current_block - 1) * block_size, SEEK_SET) ==
            -1 ||
        // TODO: REPLACE WITH K_WRITE
        write(fs_fd, buffer, bytes_read) != bytes_read) {
      P_ERRNO = P_EINVAL;
      free(buffer);
      // TODO: REPLACE WITH K_CLOSE
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
        // TODO: REPLACE WITH K_CLOSE
        close(host_fd);
        return -1;
      }

      // update the FAT chain
      fat[current_block] = next_block;
      current_block = next_block;
    }
  }

  free(buffer);
  // TODO: REPLACE WITH K_CLOSE
  close(host_fd);
  return 0;
}

// helper function to copy data from PennFAT to host OS
int copy_pennfat_to_host(const char* pennfat_filename,
                         const char* host_filename) {
  if (!is_mounted) {
    return -1;
  }

  // find the file in PennFAT
  dir_entry_t entry;
  if (find_file(pennfat_filename, &entry) == -1) {
    return -1;
  }

  // open the host file
  // TODO: REPLACE WITH K_OPEN
  int host_fd = open(host_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (host_fd == -1) {
    return -1;
  }

  // copy the data into buffer
  uint8_t* buffer = (uint8_t*)malloc(block_size);
  if (!buffer) {
    // TODO: REPLACE WITH K_CLOSE
    close(host_fd);
    return -1;
  }

  uint16_t current_block = entry.firstBlock;
  uint32_t bytes_remaining = entry.size;

  while (bytes_remaining > 0 && current_block != 0 && current_block != 0xFFFF) {
    // read from PennFAT
    // TODO: REPLACE WITH K_LSEEK
    if (lseek(fs_fd, fat_size + (current_block - 1) * block_size, SEEK_SET) ==
        -1) {
      free(buffer);
      // TODO: REPLACE WITH K_CLOSE
      close(host_fd);
      return -1;
    }

    ssize_t bytes_to_read =
        bytes_remaining < block_size ? bytes_remaining : block_size;
    // TODO: REPLACE WITH K_READ
    ssize_t bytes_read = read(fs_fd, buffer, bytes_to_read);

    if (bytes_read <= 0) {
      break;
    }

    // write to host file
    // TODO: REPLACE WITH K_WRITE
    if (write(host_fd, buffer, bytes_read) != bytes_read) {
      free(buffer);
      // TODO: REPLACE WITH K_CLOSE
      close(host_fd);
      return -1;
    }

    bytes_remaining -= bytes_read;

    // move to the next block
    current_block = fat[current_block];
  }

  free(buffer);
  // TODO: REPLACE WITH K_CLOSE
  close(host_fd);
  return 0;
}

// helper function to update the size of a file in the directory entry
int update_file_size(const char* filename, uint32_t new_size) {
  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    return -1;
  }

  // find the file
  dir_entry_t entry;
  int offset = find_file(filename, &entry);
  if (offset == -1) {
    // error is set by find_file
    return -1;
  }

  // update the size field
  entry.size = new_size;
  entry.mtime = time(NULL);

  // write the updated entry back to the directory
  // TODO: REPLACE WITH K_LSEEK
  if (lseek(fs_fd, fat_size + offset, SEEK_SET) == -1) {
    P_ERRNO = P_LSEEK;
    return -1;
  }

  // TODO: REPLACE WITH K_WRITE
  if (write(fs_fd, &entry, sizeof(entry)) != sizeof(entry)) {
    P_ERRNO = P_EINVAL;
    return -1;
  }

  return 0;
}

// helper for cat -w OUTPUT_FILE
void* cat_overwrite(char** args, fd_entry_t* fd_table) {
  dir_entry_t existing;
  if (find_file(args[2], &existing) >= 0) {
    // first check if anyone has the file open
    for (int i = 0; i < MAX_FDS; i++) {
      if (fd_table[i].in_use && strcmp(fd_table[i].filename, args[2]) == 0) {
        P_ERRNO = P_EBUSY;
        u_perror("cat");
        return NULL;
      }
    }

    // TODO: we should add a proper k_unlink function to handle this
    // for now, just mark the file entry as deleted
    // TODO: REPLACE WITH K_LSEEK
    if (lseek(fs_fd, fat_size + find_file(args[2], NULL), SEEK_SET) == -1) {
      P_ERRNO = P_LSEEK;
      u_perror("cat");
      return NULL;
    }

    char deleted = 1;  // mark as deleted
                       // TODO: REPLACE WITH K_WRITE
    if (write(fs_fd, &deleted, sizeof(deleted)) != sizeof(deleted)) {
      P_ERRNO = P_EINVAL;
      u_perror("cat");
      return NULL;
    }

    // free the FAT chain for this file
    uint16_t block = existing.firstBlock;
    while (block != 0 && block != FAT_EOF) {
      uint16_t next_block = fat[block];
      fat[block] = FAT_FREE;
      block = next_block;
    }
  }

  // allocate first block for the new file
  uint16_t first_block = allocate_block();
  if (first_block == 0) {
    P_ERRNO = P_EFULL;
    u_perror("cat");
    return NULL;
  }

  // create a new file entry with zero size initially
  if (add_file_entry(args[2], 0, first_block, TYPE_REGULAR, PERM_READ_WRITE) ==
      -1) {
    // error is set by add_file_entry
    u_perror("cat");
    fat[first_block] = FAT_FREE;  // clean up allocated block
    return NULL;
  }

  // read from stdin and write to the file
  char buffer[1024];
  uint16_t current_block = first_block;
  uint32_t total_bytes = 0;
  uint32_t block_offset = 0;

  while (1) {
    // TODO: REPLACE WITH K_READ
    ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));

    if (bytes_read <= 0) {  // eof or ctrl-d
      break;
    }

    // we may need multiple blocks to store the data
    size_t bytes_to_write = bytes_read;
    size_t buffer_offset = 0;

    while (bytes_to_write > 0) {
      // calculate how many bytes we can write to the current block
      size_t space_in_block = block_size - block_offset;
      size_t write_now =
          bytes_to_write < space_in_block ? bytes_to_write : space_in_block;

      // seek to the correct position in the current block
      // TODO: REPLACE WITH K_LSEEK
      if (lseek(fs_fd,
                fat_size + (current_block - 1) * block_size + block_offset,
                SEEK_SET) == -1) {
        P_ERRNO = P_LSEEK;
        u_perror("cat");
        update_file_size(
            args[2],
            total_bytes);  // update the file size in the directory entry
        return NULL;
      }

      // write the data
      // TODO: REPLACE WITH K_WRITE
      if (write(fs_fd, buffer + buffer_offset, write_now) != write_now) {
        P_ERRNO = P_EINVAL;
        update_file_size(args[2], total_bytes);
        return NULL;
      }

      total_bytes += write_now;
      buffer_offset += write_now;
      bytes_to_write -= write_now;
      block_offset += write_now;

      // if the current block is full and we have more data, allocate a new
      // block
      if (block_offset == block_size && bytes_to_write > 0) {
        uint16_t next_block = allocate_block();
        if (next_block == 0) {
          P_ERRNO = P_EFULL;
          update_file_size(args[2], total_bytes);
          return NULL;
        }

        // update the FAT chain
        fat[current_block] = next_block;
        current_block = next_block;
        block_offset = 0;
      }
    }
  }

  // update the file size in the directory entry
  if (update_file_size(args[2], total_bytes) == -1) {
    // error is set by update_file_size
    return NULL;
  }

  return NULL;
}
