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
//                                 GLOBALS                                    //
////////////////////////////////////////////////////////////////////////////////

int fs_fd = -1;
int block_size = 0;
int num_fat_blocks = 0;
int fat_size = 0;
uint16_t* fat = NULL;
bool is_mounted = false;
int MAX_FDS = 1024;
fd_entry_t fd_table[1024];

////////////////////////////////////////////////////////////////////////////////
//                            FD TABLE HELPERS                                //
////////////////////////////////////////////////////////////////////////////////

// helper for initializing the fd table
void init_fd_table(fd_entry_t* fd_table) {
  // STDIN (fd 0)
  fd_table[0].in_use = 1;
  strncpy(fd_table[0].filename, "<stdin>", 31);
  fd_table[0].mode = F_READ;

  // STDOUT (fd 1)
  fd_table[1].in_use = 1;
  strncpy(fd_table[1].filename, "<stdout>", 31);
  fd_table[1].mode = F_WRITE; // write-only
  
  // STDERR (fd 2)
  fd_table[2].in_use = 1;
  strncpy(fd_table[2].filename, "<stderr>", 31);
  fd_table[2].mode = F_WRITE; // write-only

  // other file descriptors (fd 3 and above)
  for (int i = 3; i < MAX_FDS; i++) {
    fd_table[i].in_use = 0;
    memset(fd_table[i].filename, 0, sizeof(fd_table[i].filename));
    fd_table[i].size = 0;
    fd_table[i].first_block = 0;
    fd_table[i].position = 0;
    fd_table[i].mode = 0;
  }
}

// helper to get a free file descriptor
int get_free_fd(fd_entry_t* fd_table) {
  for (int i = 3; i < MAX_FDS; i++) {
    if (!fd_table[i].in_use) {
      return i;
    }
  }
  return -1;
}

// helper function to allocate a block
uint16_t allocate_block() {
  for (int i = 2; i < fat_size / 2; i++) {
    if (fat[i] == FAT_FREE) {
      fat[i] = FAT_EOF;
      return i;
    }
  }
  return 0;
}

// helper function to find a file in the root directory
int find_file(const char* filename, dir_entry_t* entry) {
  if (!is_mounted) {
    P_ERRNO = P_EFS_NOT_MOUNTED;
    return -1;
  }

  // Start with root directory block (block 1)
  uint16_t current_block = 1;
  int offset_in_block = 0;
  int absolute_offset = 0;
  dir_entry_t dir_entry;

  while (1) {
    // Position at the start of current block
    if (lseek(fs_fd, fat_size + (current_block - 1) * block_size, SEEK_SET) == -1) {
      P_ERRNO = P_ELSEEK;
      return -1;
    }

    // reset offset for new block
    offset_in_block = 0;

    // calculate the absolute offset
    absolute_offset = fat_size + (current_block - 1) * block_size;

    // search current block
    while (offset_in_block < block_size) {
      if (read(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
        P_ERRNO = P_EREAD;
        return -1;
      }

      // check if we've reached the end of directory
      if (dir_entry.name[0] == 0) {
        break;
      }

      // check if this is a deleted entry
      if (dir_entry.name[0] == 1 || dir_entry.name[0] == 2) {
        offset_in_block += sizeof(dir_entry);
        absolute_offset += sizeof(dir_entry);
        continue;
      }

      // check if we found the file
      if (strcmp(dir_entry.name, filename) == 0) {
        if (entry) {
          memcpy(entry, &dir_entry, sizeof(dir_entry));
        }
        return absolute_offset; // return the absolute file offset
      }

      offset_in_block += sizeof(dir_entry);
      absolute_offset += sizeof(dir_entry);
    }

    // if we've reached the end of the current block, check if there's a next block
    if (fat[current_block] != FAT_EOF) {
      current_block = fat[current_block];
      continue;
    }

    // no more blocks to search
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
    P_ERRNO = P_EFS_NOT_MOUNTED;
    return -1;
  }

  // check if file already exists
  dir_entry_t existing;
  if (find_file(filename, &existing) >= 0) {
    P_ERRNO = P_EEXIST;
    return -1;
  }

  // start with root directory block (block 1)
  uint16_t current_block = 1;
  int offset = 0;
  dir_entry_t dir_entry;

  while (1) {
    // position at the start of current block of the root directory
    if (lseek(fs_fd, fat_size + (current_block - 1) * block_size, SEEK_SET) == -1) {
      P_ERRNO = P_ELSEEK;
      return -1;
    }

    // reset offset for new block
    offset = 0;

    // search current block for free slot
    while (offset < block_size) {
      if (read(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
        P_ERRNO = P_EINVAL;
        return -1;
      }

      // found a free slot
      if (dir_entry.name[0] == 0 || dir_entry.name[0] == 1) {
        // initialize the new entry
        memset(&dir_entry, 0, sizeof(dir_entry));
        strncpy(dir_entry.name, filename, 31);
        dir_entry.size = size;
        dir_entry.firstBlock = first_block;
        dir_entry.type = type;
        dir_entry.perm = perm;
        dir_entry.mtime = time(NULL);

        // write the entry
        if (lseek(fs_fd, fat_size + (current_block - 1) * block_size + offset, SEEK_SET) == -1) {
          P_ERRNO = P_ELSEEK;
          return -1;
        }
        if (write(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
          P_ERRNO = P_EWRITE;
          return -1;
        }

        return offset;
      }

      offset += sizeof(dir_entry);
    }

    // current block is full, check if there's a next block
    if (fat[current_block] != FAT_EOF) {
      current_block = fat[current_block];
      continue;
    }

    // haven't added the file yet, so we need to allocate new blocks
    uint16_t new_block = allocate_block();
    if (new_block == 0) {
      P_ERRNO = P_EFULL;
      return -1;
    }

    // chain the new block
    fat[current_block] = new_block;
    fat[new_block] = FAT_EOF;

    // initialize new block with zeros
    uint8_t* zero_block = calloc(block_size, 1);
    if (!zero_block) {
      P_ERRNO = P_EINVAL;
      return -1;
    }

    // write this new block to the file system
    if (lseek(fs_fd, fat_size + (new_block - 1) * block_size, SEEK_SET) == -1) {
      P_ERRNO = P_ELSEEK;
      free(zero_block);
      return -1;
    }
    if (write(fs_fd, zero_block, block_size) != block_size) {
      P_ERRNO = P_EWRITE;
      free(zero_block);
      return -1;
    }

    free(zero_block);

    // initialize the new entry
    memset(&dir_entry, 0, sizeof(dir_entry));
    strncpy(dir_entry.name, filename, 31);
    dir_entry.size = size;
    dir_entry.firstBlock = first_block;
    dir_entry.type = type;
    dir_entry.perm = perm;
    dir_entry.mtime = time(NULL);

    // write the new entry at the start of the new block in the file system
    if (lseek(fs_fd, fat_size + (new_block - 1) * block_size, SEEK_SET) == -1) {
      P_ERRNO = P_ELSEEK;
      return -1;
    }
    if (write(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
      P_ERRNO = P_EWRITE;
      return -1;
    }

    return 0;
  }
}

int mark_entry_as_deleted(dir_entry_t* entry, int absolute_offset) {
  if (!is_mounted || entry == NULL || absolute_offset < 0) {
    P_ERRNO = P_EINVAL;
    return -1;
  }

  // free the blocks
  uint16_t block = entry->firstBlock;
  while (block != 0 && block != FAT_EOF) {
    uint16_t next_block = fat[block];
    fat[block] = FAT_FREE;
    block = next_block;
  }

  dir_entry_t deleted_entry = *entry;
  deleted_entry.name[0] = 1;

  // update the file system
  if (lseek(fs_fd, absolute_offset, SEEK_SET) == -1) {
    P_ERRNO = P_ELSEEK;
    return -1;
  }
  if (write(fs_fd, &deleted_entry, sizeof(deleted_entry)) !=
      sizeof(deleted_entry)) {
    P_ERRNO = P_EINVAL;
    return -1;
  }

  // mark the passed entry as deleted
  entry->name[0] = 1;
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
//                                CP HELPERS                                  //
////////////////////////////////////////////////////////////////////////////////

// helper function to copy data from host OS to PennFAT
int copy_host_to_pennfat(const char* host_filename,
                         const char* pennfat_filename) {
  if (!is_mounted) {
    P_ERRNO = P_EFS_NOT_MOUNTED;
    return -1;
  }

  // open the host file
  int host_fd = open(host_filename, O_RDONLY);
  if (host_fd == -1) {
    P_ERRNO = P_EOPEN;
    return -1;
  }

  // determine file size by seeking to the end and getting position
  off_t host_file_size_in_bytes = lseek(host_fd, 0, SEEK_END);
  if (host_file_size_in_bytes == -1) {
    P_ERRNO = P_ELSEEK;
    close(host_fd);
    return -1;
  }

  // go back to beginning of file for reading
  if (lseek(host_fd, 0, SEEK_SET) == -1) {
    P_ERRNO = P_ELSEEK;
    close(host_fd);
    return -1;
  }

  // open the destination file in PennFAT
  int pennfat_fd = k_open(pennfat_filename, F_WRITE);
  if (pennfat_fd < 0) {
    // error code is already set by k_open
    close(host_fd);
    return -1;
  }

  // copy the data into this buffer
  uint8_t* buffer = (uint8_t*)malloc(block_size);
  if (!buffer) {
    P_ERRNO = P_EMALLOC;
    k_close(pennfat_fd);
    close(host_fd);
    return -1;
  }

  uint32_t bytes_remaining = host_file_size_in_bytes;

  // read from host file
  while (bytes_remaining > 0) {
    // ensure bytes to read never exceeds the block size
    ssize_t bytes_to_read = bytes_remaining < block_size ? bytes_remaining : block_size;
    ssize_t bytes_read = read(host_fd, buffer, bytes_to_read);

    if (bytes_read <= 0) {
      break;  // reached eof or error
    }

    // write to pennfat_fd using k_write
    if (k_write(pennfat_fd, (const char*)buffer, bytes_read) != bytes_read) {
      // errors are already set in the kernel functions
      free(buffer);
      k_close(pennfat_fd);
      close(host_fd);
      return -1;
    }

    bytes_remaining -= bytes_read;
  }

  free(buffer);
  k_close(pennfat_fd);
  close(host_fd);
  return 0;
}

// helper function to copy data from PennFAT to host OS
int copy_pennfat_to_host(const char* pennfat_filename,
                         const char* host_filename) {
  if (!is_mounted) {
    P_ERRNO = P_EFS_NOT_MOUNTED;
    return -1;
  }

  // open the PennFAT file
  int pennfat_fd = k_open(pennfat_filename, F_READ);
  if (pennfat_fd < 0) {
    // error already set by k_open
    return -1;
  }

  // open the host file
  int host_fd = open(host_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (host_fd == -1) {
    P_ERRNO = P_EINVAL;
    k_close(pennfat_fd);
    return -1;
  }

  // allocate buffer for data transfer
  char buffer[4096];  // TODO: might want to malloc for buffer
  ssize_t bytes_read;

  // read from PennFAT file and write to host file
  while ((bytes_read = k_read(pennfat_fd, sizeof(buffer), buffer)) > 0) {
    if (write(host_fd, buffer, bytes_read) != bytes_read) {
      P_ERRNO = P_EINVAL;
      close(host_fd);
      k_close(pennfat_fd);
      return -1;
    }
  }

  // check for read error
  if (bytes_read < 0) {
    // error already set by k_read
    close(host_fd);
    k_close(pennfat_fd);
    return -1;
  }

  // cleanup
  close(host_fd);
  k_close(pennfat_fd);
  return 0;
}

int copy_source_to_dest(const char* source_filename,
                        const char* dest_filename) {
  if (!is_mounted) {
    P_ERRNO = P_EFS_NOT_MOUNTED;
    return -1;
  }

  // open the source file
  int source_fd = k_open(source_filename, F_READ);
  if (source_fd < 0) {
    return -1;
  }

  // open the destination file
  int dest_fd = k_open(dest_filename, F_WRITE);
  if (dest_fd < 0) {
    k_close(source_fd);
    return -1;
  }

  // read from source to destination
  char buffer[4096];
  ssize_t bytes_read;
  while ((bytes_read = k_read(source_fd, sizeof(buffer), buffer)) > 0) {
    if (k_write(dest_fd, buffer, bytes_read) != bytes_read) {
      k_close(source_fd);
      k_close(dest_fd);
      return -1;
    }
  }

  // check for read error
  if (bytes_read < 0) {
    k_close(source_fd);
    k_close(dest_fd);
    return -1;
  }

  // cleanup
  k_close(source_fd);
  k_close(dest_fd);
  return 0;
}
