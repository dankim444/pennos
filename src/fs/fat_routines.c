#include "fat_routines.h"
#include "../lib/pennos-errno.h"
#include "../shell/builtins.h"
#include "fs_helpers.h"
#include "fs_kfuncs.h"

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
//                                 ROUTINES                                   //
////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a PennFAT filesystem in the file named fs_name at the OS-level
 */
int mkfs(const char* fs_name, int num_blocks, int blk_size) {
  // validate arguments
  if (num_blocks < 1 || num_blocks > 32) {
    return -1;
  }
  if (blk_size < 0 || blk_size > 4) {
    return -1;
  }

  // determine the file system size
  int block_sizes[] = {256, 512, 1024, 2048, 4096};
  int actual_block_size = block_sizes[blk_size];
  int fat_size = num_blocks * actual_block_size;
  int fat_entries = fat_size / 2;
  int data_blocks = fat_entries - 1;  // subtract 1 for the root directory block
  size_t filesystem_size = fat_size + (actual_block_size * data_blocks);

  // create the file for the filesystem
  int fd = open(fs_name, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (fd == -1) {
    return -1;
  }

  // extend the file to the required size
  if (ftruncate(fd, filesystem_size) == -1) {
    close(fd);
    return -1;
  }

  // allocate the FAT
  uint16_t* temp_fat = (uint16_t*)calloc(fat_entries, sizeof(uint16_t));
  if (!temp_fat) {
    close(fd);
    return -1;
  }

  // initialize all FAT entries to free
  for (int i = 0; i < fat_entries; i++) {
    temp_fat[i] = FAT_FREE;
  }

  // initialize the first two entries of FAT (metadata and root directory)
  temp_fat[0] = (num_blocks << 8) | blk_size;
  temp_fat[1] = FAT_EOF; // root directory is only stored in one block

  // write the FAT to the file
  if (write(fd, temp_fat, fat_size) != fat_size) {
    free(temp_fat);
    close(fd);
    return -1;
  }

  // initialize the root directory
  uint8_t* root_dir = (uint8_t*)calloc(actual_block_size, 1);
  if (lseek(fd, fat_size, SEEK_SET) == -1 ||
      write(fd, root_dir, actual_block_size) != actual_block_size) {
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
int mount(const char* fs_name) {
  // check if a filesystem is already mounted
  if (is_mounted) {
    P_ERRNO = P_EBUSY;
    return -1;
  }

  // open the file with fs_name + set the global fs_fd
  fs_fd = open(fs_name, O_RDWR);
  if (fs_fd == -1) {
    P_ERRNO = P_ENOENT;
    return -1;
  }

  // read the first two bytes to get configuration
  uint16_t config;
  if (read(fs_fd, &config, sizeof(config)) != sizeof(config)) {
    P_ERRNO = P_READ;
    close(fs_fd);
    fs_fd = -1;
    return -1;
  }

  // extract size information from first two bytes
  num_fat_blocks = (config >> 8) & 0xFF;  // MSB
  int block_size_config = config & 0xFF;  // LSB

  // determine actual block size from configuration
  int block_sizes[] = {256, 512, 1024, 2048, 4096};
  block_size = block_sizes[block_size_config];
  fat_size = num_fat_blocks * block_size;

  // map the FAT into memory
  if (lseek(fs_fd, 0, SEEK_SET) == -1) {
    P_ERRNO = P_LSEEK;
    close(fs_fd);
    fs_fd = -1;
    return -1;
  }

  fat = mmap(NULL, fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd, 0);
  if (fat == MAP_FAILED) {
    P_ERRNO = P_MAP;
    close(fs_fd);
    fs_fd = -1;
    return -1;
  }

  // initialize the fd table
  init_fd_table(fd_table);
  is_mounted = true;
  return 0;
}

/**
 * Unmounts the current filesystem and reset variables.
 */
int unmount() {
  // first check that a file system is actually mounted
  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    return -1;
  }

  // unmap the FAT
  if (fat != NULL) {
    if (munmap(fat, fat_size) == -1) {
      P_ERRNO = P_MAP;
      return -1;
    }
    fat = NULL;
  }

  // close fs_fd
  if (fs_fd != -1) {
    if (close(fs_fd) == -1) {
      P_ERRNO = P_EBADF;
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

/**
 * Concatenates and displays files.
 */
void* cat(void* arg) {
  char** args = (char**)arg;

  // verify that the file system is mounted
  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    u_perror("cat");
    return NULL;
  }

  // early return if there is nothing after cat
  if (args[1] == NULL) {
    P_ERRNO = P_EINVAL;
    u_perror("cat");
    return NULL;
  }

  // check for output file with -w or -a flag
  int out_fd = -1;
  int out_mode = 0;

  // search for output redirection
  // handles cat -w/-a OUTPUT_FILE and cat FILE ... -w/-a OUTPUT_FILE
  int i;
  for (i = 1; args[i] != NULL; i++) {
    if (strcmp(args[i], "-w") == 0 && args[i + 1] != NULL) {
      out_mode = F_WRITE;
      out_fd = k_open(args[i + 1], F_WRITE);
      if (out_fd < 0) {
        // error set by k_open
        u_perror("cat");
        return NULL;
      }
      break;
    } else if (strcmp(args[i], "-a") == 0 && args[i + 1] != NULL) {
      out_mode = F_APPEND;
      out_fd = k_open(args[i + 1], F_APPEND);
      if (out_fd < 0) {
        u_perror("cat");
        return NULL;
      }
      break;
    }
  }

  // if no output redirection found, use STDOUT
  if (out_fd < 0) {
    out_fd = STDOUT_FILENO;
  }

  // handle small case: cat -w OUTPUT_FILE or cat -a OUTPUT_FILE (read from
  // stdin)
  if ((strcmp(args[1], "-w") == 0 || strcmp(args[1], "-a") == 0) &&
      args[2] != NULL && args[3] == NULL) {
    char buffer[1024];
    while (1) {
      ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
      if (bytes_read <= 0) {
        break;  // eof or error
      }

      if (k_write(out_fd, buffer, bytes_read) != bytes_read) {
        u_perror("cat");
        if (out_fd != STDOUT_FILENO) {
          k_close(out_fd);
        }
        return NULL;
      }
    }

    if (out_fd != STDOUT_FILENO) {
      k_close(out_fd);
    }
    return NULL;
  }

  // handle concatenating one or more files: cat FILE ... [-w/-a OUTPUT_FILE]
  int start = 1;
  int end = i - 1;

  if (out_mode != 0) {
    end = i - 1;  // skip the output redirection arguments
  }

  // process each input file
  for (i = start; i <= end; i++) {
    // skip the redirection flags and their arguments
    if (strcmp(args[i], "-w") == 0 || strcmp(args[i], "-a") == 0) {
      i++;
      continue;
    }

    int in_fd = k_open(args[i], F_READ);
    if (in_fd < 0) {
      // set error code
      u_perror("cat");
      continue;
    }

    // copy file content to output
    char buffer[1024];
    int bytes_read;

    while ((bytes_read = k_read(in_fd, sizeof(buffer), buffer)) > 0) {
      if (k_write(out_fd, buffer, bytes_read) != bytes_read) {
        u_perror("cat");
        k_close(in_fd);
        if (out_fd != STDOUT_FILENO) {
          k_close(out_fd);
        }
        return NULL;
      }
    }

    if (bytes_read < 0) {
      u_perror("cat");
    }

    k_close(in_fd);
  }

  // close output file if not stdout
  if (out_fd != STDOUT_FILENO) {
    k_close(out_fd);
  }

  return NULL;
}

/**
 * List all files in the directory.
 */
// TODO: REWORK --> need to use k_ functions or wrap entirely with k_ls
void* ls(void* arg) {
  // will eventually need args for ls-ing certain files
  // char **args = (char **)arg;

  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    u_perror("ls");
    return NULL;
  }

  // Start with root directory block (block 1)
  uint16_t current_block = 1;
  int offset = 0;
  dir_entry_t dir_entry;

  while (1) {
    // Position at the start of current block
    if (lseek(fs_fd, fat_size + (current_block - 1) * block_size, SEEK_SET) ==
        -1) {
      P_ERRNO = P_LSEEK;
      u_perror("ls");
      return NULL;
    }

    // Reset offset for new block
    offset = 0;

    // Search current block
    while (offset < block_size) {
      if (read(fs_fd, &dir_entry, sizeof(dir_entry)) != sizeof(dir_entry)) {
        P_ERRNO = P_READ;
        u_perror("ls");
        return NULL;
      }

      // check if we've reached the end of directory
      if (dir_entry.name[0] == 0) {
        break;
      }

      // skip deleted entries
      if (dir_entry.name[0] == 1 || dir_entry.name[0] == 2) {
        offset += sizeof(dir_entry);
        continue;
      }

      // format permission string
      char perm_str[4] = "---";
      if (dir_entry.perm & PERM_READ)
        perm_str[0] = 'r';
      if (dir_entry.perm & PERM_WRITE)
        perm_str[1] = 'w';
      // if (dir_entry.perm & PERM_EXEC) perm_str[2] = 'x'; // do we need x?

      // format time
      struct tm* tm_info = localtime(&dir_entry.mtime);
      char time_str[50];
      strftime(time_str, sizeof(time_str), "%b %d %H:%M:%S %Y",
               tm_info);  // TODO: check if we're allowed to use strftime

      // print entry details
      printf("%2d -%s- %6d %s %s\n", dir_entry.firstBlock, perm_str,
             dir_entry.size, time_str, dir_entry.name);  // TODO: replace printf
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

  return NULL;
}

/**
 * Creates files or updates timestamps.
 *
 * For each file argument, creates the file if it doesn't exist,
 * or updates its timestamp if it already exists.
 */
void* touch(void* arg) {
  char** args = (char**)arg;

  // verify that the file system is mounted
  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    u_perror("touch");
    return NULL;
  }

  // check if we have any arguments
  if (args[1] == NULL) {
    P_ERRNO = P_EINVAL;
    u_perror("touch: missing file operand");
    return NULL;
  }

  // process each file argument
  for (int i = 1; args[i] != NULL; i++) {
    // print_fat_state("Before touching file");
    dir_entry_t entry;
    int entry_offset = find_file(args[i], &entry);

    if (entry_offset >= 0) {
      // file exists, just update the timestamp
      entry.mtime = time(NULL);

      // write the updated entry back to the directory
      // REPLACE WITH K_LSEEK
      if (lseek(fs_fd, entry_offset, SEEK_SET) == -1) {
        P_ERRNO = P_LSEEK;
        u_perror("touch");
        continue;
      }

      // REPLACE WITH K_WRITE
      if (write(fs_fd, &entry, sizeof(entry)) != sizeof(entry)) {
        P_ERRNO = P_EINVAL;
        u_perror("touch");
        continue;
      }
    } else {
      // file doesn't exist, create a new empty file

      // first check if directory is full
      if (P_ERRNO == P_EFULL) {
        u_perror("touch");
        continue;
      }

      // add the file entry to the directory with first_block = 0
      // blocks will be allocated when data is written
      if (add_file_entry(args[i], 0, 0, TYPE_REGULAR,
                         PERM_READ_WRITE) == -1) {
        u_perror("touch");
        continue;
      }
    }
    // print_fat_state("After touching file");
  }

  return NULL;
}

/**
 * Renames files.
 */
void* mv(void* arg) {
  char** args = (char**)arg;

  // verify that the file system is mounted
  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    u_perror("mv");
    return NULL;
  }

  // check if we have both source and destination arguments
  if (args[1] == NULL || args[2] == NULL) {
    P_ERRNO = P_EINVAL;
    u_perror("mv");
    return NULL;
  }

  char* source = args[1];
  char* dest = args[2];

  // check if they're trying to rename to the same name
  if (strcmp(source, dest) == 0) {
    return NULL;
  }

  // check if source file exists
  dir_entry_t source_entry;
  int source_offset = find_file(source, &source_entry);
  if (source_offset < 0) {
    // set error code
    // TODO: replace printf with u_perror
    printf("mv: cannot rename %s to %s\n", source, dest);
    return NULL;
  }

  // check if the destination file already exists
  dir_entry_t dest_entry;
  int dest_offset = find_file(dest, &dest_entry);
  if (dest_offset >= 0) {
    // check if the destination file is currently open by any process
    for (int i = 0; i < MAX_FDS; i++) {
      if (fd_table[i].in_use && strcmp(fd_table[i].filename, dest) == 0) {
        P_ERRNO = P_EBUSY;
        u_perror("mv");
        return NULL;
      }
    }

    // if destination file exists, delete it
    if (mark_entry_as_deleted(&dest_entry, dest_offset) != 0) {
      u_perror("mv");
      return NULL;
    }
  }

  // rename file
  strncpy(source_entry.name, dest, sizeof(source_entry.name) - 1);
  source_entry.name[sizeof(source_entry.name) - 1] =
      '\0';  // ensure null termination

  // write the updated entry back to disk
  if (lseek(fs_fd, source_offset, SEEK_SET) == -1) {
    P_ERRNO = P_LSEEK;
    u_perror("mv");
    return NULL;
  }

  if (write(fs_fd, &source_entry, sizeof(source_entry)) !=
      sizeof(source_entry)) {
    P_ERRNO = P_EINVAL;
    u_perror("mv");
    return NULL;
  }

  return NULL;
}

/**
 * Copies the source file to the destination.
 */
void* cp(void* arg) {
  char** args = (char**)arg;

  // check that we have enough arguments
  if (args[1] == NULL || args[2] == NULL) {
    P_ERRNO = P_EINVAL;
    u_perror("cp");
    return NULL;
  }

  // cp -h SOURCE DEST
  if (strcmp(args[1], "-h") == 0) {
    if (args[2] == NULL || args[3] == NULL) {
      P_ERRNO = P_EINVAL;
      u_perror("cp");
      return NULL;
    }

    if (copy_host_to_pennfat(args[2], args[3]) != 0) {
      // error should aready be set by the function
      u_perror("cp");
      return NULL;
    }
    return NULL;
  }

  // cp SOURCE -h DEST
  if (args[2] != NULL && strcmp(args[2], "-h") == 0) {
    if (args[3] == NULL) {
      P_ERRNO = P_EINVAL;
      u_perror("cp");
      return NULL;
    }

    if (copy_pennfat_to_host(args[1], args[3]) != 0) {
      // error set by function
      u_perror("cp");
      return NULL;
    }
    return NULL;
  }

  // cp SOURCE DEST
  if ((args[1] != NULL && strcmp(args[1], "-h") != 0) &&
      (args[2] != NULL && strcmp(args[2], "-h") != 0) && args[3] == NULL) {
    if (copy_source_to_dest(args[1], args[2]) != 0) {
      // error set by functions
      u_perror("cp");
      return NULL;
    }
    return NULL;
  }

  P_ERRNO = P_EUNKNOWN;
  u_perror("cp");
  return NULL;
}

/**
 * Removes files.
 */
void* rm(void* arg) {
  char** args = (char**)arg;

  // verify that the file system is mounted
  if (!is_mounted) {
    P_ERRNO = P_FS_NOT_MOUNTED;
    u_perror("rm");
    return NULL;
  }

  // check if we have any arguments
  if (args[1] == NULL) {
    P_ERRNO = P_EINVAL;
    u_perror("rm: missing file operand");
    return NULL;
  }

  // process each file argument
  for (int i = 1; args[i] != NULL; i++) {
    // find the file in the directory
    dir_entry_t entry;
    int entry_offset = find_file(args[i], &entry);

    if (entry_offset < 0) {
      // file doesn't exist
      P_ERRNO = P_ENOENT;
      u_perror("rm");
      continue;
    }

    // check if file is currently open
    for (int j = 0; j < MAX_FDS; j++) {
      if (fd_table[j].in_use && strcmp(fd_table[j].filename, args[i]) == 0) {
        P_ERRNO = P_EBUSY;
        u_perror("rm");
        continue;
      }
    }

    // mark the directory entry as deleted
    // TODO: REPLACE WITH K_LSEEK
    if (lseek(fs_fd, entry_offset, SEEK_SET) == -1) {
      P_ERRNO = P_LSEEK;
      u_perror("rm");
      continue;
    }

    char deleted = 1;  // mark as deleted
    // TODO: REPLACE WITH K_WRITE
    if (write(fs_fd, &deleted, sizeof(deleted)) != sizeof(deleted)) {
      P_ERRNO = P_EINVAL;
      u_perror("rm");
      continue;
    }

    // free the FAT chain for this file
    uint16_t block = entry.firstBlock;
    while (block != 0 && block != FAT_EOF) {
      uint16_t next_block = fat[block];
      fat[block] = FAT_FREE;
      block = next_block;
    }
  }

  return NULL;
}

// void* chmod(void *arg) {
//     return 0;
// }
