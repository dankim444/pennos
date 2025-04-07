#ifndef FS_FAT_H_
#define FS_FAT_H_

#include <stdbool.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
//                          PENNFAT DEFINITIONS                               //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Special FAT entry value indicating the end of a file chain.
 *
 * When this value is encountered in the FAT table, it signifies that
 * the current block is the last block of a file.
 */
#define FAT_EOF 0xFFFF

/**
 * @brief Special FAT entry value indicating a free block.
 *
 * When this value is found in the FAT table, it indicates that
 * the corresponding block is not allocated to any file and is available
 * for allocation.
 */
#define FAT_FREE 0x0000

////////////////////////////////////////////////////////////////////////////////
//                          PENNFAT FUNCTIONS                                 //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Initialize a new FAT file system or format an existing one.
 *
 * This function creates a new FAT file system on the specified file.
 * If the file already exists, it will be formatted (all data will be lost).
 * If the file does not exist, it will be created with the default size.
 *
 * @param fs_name The name of the file to use as the file system.
 * @param num_blocks The total number of blocks in the file system.
 * @param block_size The size of a block in bytes.
 * @return On success, returns 0.
 *         On error, returns -1 and sets P_ERRNO to indicate the error.
 */
int fat_init(const char* fs_name, uint16_t num_blocks, uint32_t block_size);

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
int fat_load(const char* fs_name);

/**
 * @brief Unmount the currently mounted FAT file system.
 *
 * This function synchronizes any cached data to disk and releases
 * all resources associated with the file system. After calling this
 * function, no further file system operations should be performed
 * until fat_init() or fat_load() is called again.
 */
void fat_unmount();

/**
 * @brief Synchronize the FAT table to disk.
 *
 * This function ensures that any in-memory changes to the FAT table
 * are written to persistent storage.
 *
 * @return On success, returns 0.
 *         On error, returns -1 and sets P_ERRNO to indicate the error.
 */
int fat_save();

/**
 * @brief Get the value of a specific entry in the FAT table.
 *
 * This function retrieves the FAT entry for the specified block index.
 * This value indicates either the next block in the chain, FAT_EOF to
 * indicate end of file, or FAT_FREE to indicate the block is not allocated.
 *
 * @param index The block index to look up in the FAT.
 * @return The value stored in the FAT for the specified block.
 *         Returns FAT_EOF if the index is invalid or the file system is not
 * mounted.
 */
uint16_t fat_get_block(uint16_t index);

/**
 * @brief Set the value of a specific entry in the FAT table.
 *
 * This function updates the FAT entry for the specified block index.
 *
 * @param index The block index to update in the FAT.
 * @param value The new value to store (next block index, FAT_EOF, or FAT_FREE).
 */
void fat_set_block(uint16_t index, uint16_t value);

/**
 * @brief Find a free block in the FAT.
 *
 * This function scans the FAT table to find a block that is marked as free
 * (ie. FAT_FREE) and can be allocated for file storage.
 *
 * @return On success, returns the index of a free block.
 *         If no free blocks are available or the file system is not mounted,
 *         returns -1.
 */
int fat_find_free_block();

/**
 * @brief Get the block size used by the file system.
 *
 * This function returns the size in bytes of each block in the FAT file system.
 * All file operations work with this block size as the basic unit of
 * allocation.
 *
 * @return The block size in bytes.
 */
int fat_get_block_size();

/**
 * @brief Get the offset to the data region.
 *
 * This function returns the byte offset from the beginning of the file system
 * to the start of the data region where file contents are stored.
 *
 * @return On success, returns the byte offset to the data region.
 *         If the file system is not mounted, returns -1.
 */
int fat_get_data_offset();

/**
 * @brief Get the total number of entries in the FAT.
 *
 * This function returns the total number of entries managed by the FAT,
 * including both allocated and free blocks.
 *
 * @return On success, returns the number of entries in the FAT.
 *         If the file system is not mounted, returns -1.
 */
int fat_get_num_entries();

/**
 * @brief Get the size of the FAT table in bytes.
 *
 * This function returns the total size in bytes of the FAT table in memory.
 * This is useful for memory management and debugging purposes.
 *
 * @return On success, returns the size of the FAT in bytes.
 *         If the file system is not mounted, returns -1.
 */
int fat_get_fat_size_bytes();

#endif
