// fat block management, helpers
// global file descriptor table
// directory structure

#include "fat.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>

// TODO: Implement FAT initialization, parsing MSB/LSB
// TODO: Implement finding next free block
// TODO: Implement mmap'ing file and loading FAT into memory
// TODO: Implement accessor and mutator helpers
// TODO: Implement unmount logic and cleanup

#define PERM 0644

// FAT State Global Variables
static int fs_fd = -1;        // File descriptor for the mounted filesystem
static uint16_t* fat = NULL;  // Pointer to memory-mapped FAT table
static uint16_t num_fat_entries = 0;  // Number of entries in the FAT
static uint32_t block_size = 0;       // Size of each block in bytes
static uint32_t fat_size_bytes = 0;   // Size of the FAT table in bytes
static bool is_mounted = false;  // Flag indicating if a filesystem is mounted

typedef struct {
  char name[32];        // Null-terminated file name
  uint32_t size;        // File size in bytes
  uint16_t firstBlock;  // First block number of the file
  uint8_t type;         // File type
  uint8_t perm;         // File permissions
  time_t mtime;         // Modification time
  char reserved[16];
} dir_entry_t;

/**
 * @param fs_name The name of the file to use as the file system.
 * @param num_blocks The total number of blocks in the file system. (MSB)
 * @param block_size The size of a block in bytes. (LSB)
 * Ex. x2004: MSB 20 -> 32 blocks, LSB 04, 4096 bytes
 * @return On success, returns 0.
 *         On error, returns -1 and sets P_ERRNO to indicate the error.
 */
int fat_init(const char* fs_name, uint16_t num_blocks, uint32_t block_size) {
  if (is_mounted) {
    return -1;
  }
  if (!fs_name || num_blocks < 1 || num_blocks > 32 || block_size < 0 ||
      block_size > 4) {
    return -1;
  }

  uint16_t block_size_bytes;
  switch (block_size) {
    case 0:
      block_size_bytes = 256;
      break;
    case 1:
      block_size_bytes = 512;
      break;
    case 2:
      block_size_bytes = 1024;
      break;
    case 3:
      block_size_bytes = 2048;
      break;
    case 4:
      block_size_bytes = 4096;
      break;
    default:
      return -1;
  }

  fs_fd = open(fs_name, O_RDWR | O_CREAT | O_TRUNC, PERM);

  if (fs_fd == -1) {
    return -1;
  }

  fat_size_bytes = num_blocks * block_size_bytes;
  num_fat_entries = fat_size_bytes / 2;

  uint32_t fat_total_size =
      (block_size_bytes * (num_fat_entries - 1)) + fat_size_bytes;

  // Resize filesystem file to be the total size
  if (ftruncate(fs_fd, fat_total_size) == -1) {
    close(fs_fd);
    fs_fd = -1;
    return -1;
  }

  fat = mmap(
      NULL, fat_size_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd,
      0);  // MAP_SHARED -> writes to the memory are reflected back to the file

  if (fat == MAP_FAILED) {
    close(fs_fd);
    fs_fd = -1;
    return -1;
  }

  memset(fat, 0, fat_size_bytes);

  // MSB|LSB
  fat[0] = (num_blocks << 8) | block_size;
  fat[1] = FAT_EOF;

  // Block 1 as Root Directory block
  char* root_dir = (char*)calloc(1, block_size_bytes);
  if (!root_dir) {
    munmap(fat, fat_size_bytes);
    close(fs_fd);
    fs_fd = -1;
    return -1;
  }

  // Write root dir to disk
  if (lseek(fs_fd, fat_size_bytes, SEEK_SET) == -1 ||
      write(fs_fd, root_dir, block_size_bytes) != block_size_bytes) {
    free(root_dir);
    munmap(fat, fat_size_bytes);
    close(fs_fd);
    fs_fd = -1;
    return -1;
  }

  free(root_dir);
  block_size = block_size_bytes;
  is_mounted = true;

  return 0;
}

/**
 * @brief Load an existing FAT file system.
 *
 * This function mounts an existing FAT file system from the specified file.
 * It reads the superblock and FAT data into memory for fast access.
 *
 * @param fs_name The name of the file containing the file system.
 * @return On success, returns 0.
 *         On error, returns -1 and sets P_ERRNO to indicate the error.
 */
int fat_load(const char* fs_name) {
  if (!fs_name || is_mounted) {
    return -1;
  }

  fs_fd = open(fs_name, O_RDWR);
  if (fs_fd == -1) {
    return -1;
  }

  // 4bytes -> uint16
  void* fat_map =
      mmap(NULL, sizeof(uint16_t), PROT_READ, MAP_PRIVATE, fs_fd, 0);

  if (fat_map == MAP_FAILED) {
    close(fs_fd);
    fs_fd = -1;
    return -1;
    // do P_ERRNO stuff later TODO
  }

  uint16_t load_fat_metadata = *(uint16_t*)fat_map;

  uint8_t block_size = load_fat_metadata & 0xFF;
  uint8_t num_blocks = (load_fat_metadata >> 8) & 0xFF;

  munmap(fat_map, sizeof(uint16_t));

  if (num_blocks < 1 || num_blocks > 32 || block_size < 0 || block_size > 4) {
    return -1;
  }

  int block_size_bytes;
  switch (block_size) {
    case 0:
      block_size_bytes = 256;
    case 1:
      block_size_bytes = 512;
    case 2:
      block_size_bytes = 1024;
    case 3:
      block_size_bytes = 2048;
    case 4:
      block_size_bytes = 4096;
    default:
      return -1;
  }

  fat_size_bytes = num_blocks * block_size_bytes;
  num_fat_entries = fat_size_bytes / 2;

  fat = mmap(
      NULL, fat_size_bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fs_fd,
      0);  // MAP_SHARED -> writes to the memory are reflected back to the file

  if (fat == MAP_FAILED) {
    close(fs_fd);
    fs_fd = -1;
    return -1;
  }

  is_mounted = true;

  return 0;  // TODO | NEED FOR TESTING
}

/**
 * This function synchronizes any cached data to disk and releases
 * all resources associated with the file system. After calling this
 * function, no further file system operations should be performed
 * until fat_init() or fat_load() is called again.
 */
void fat_unmount() {
  // TODO NEXT
  if (!is_mounted) {
    return;
  }

  // fat_sync to disk
  // what do i even do if this fails realistically
  if (fat_save() == -1) {
    munmap(fat, fat_size_bytes);
    fat = NULL;
    close(fs_fd);
    fs_fd = -1;
  }

  if (fat != NULL) {
    munmap(fat, fat_size_bytes);
    fat = NULL;
  }

  if (fs_fd != -1) {
    close(fs_fd);
    fs_fd = -1;
  }

  is_mounted = false;
  num_fat_entries = 0;
  fat_size_bytes = 0;
  block_size = 0;
}

int fat_save() {
  if (!is_mounted || fat == NULL) {
    return -1;
  }

  if (msync(fat, fat_size_bytes, MS_SYNC) == -1) {
    return -1;
  }
  return 0;
}

uint16_t fat_get_block(uint16_t index) {
  return index;
}

void fat_set_block(uint16_t index, uint16_t value) {
  return;
}

int fat_find_free_block() {
  return -1;
}

int fat_get_block_size() {
  return -1;
}

int fat_get_data_offset() {
  return -1;
}

int fat_get_num_entries() {
  return -1;
}

int fat_get_fat_size_bytes() {
  return -1;
}
