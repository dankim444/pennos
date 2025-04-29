/* CS5480 PennOS Group 61
 * Authors: Dan Kim and Kevin Zhou
 * Purpose: Defines the global variables for the mounted filesystem and additional
 *          helpers to implement the routines.
 */

#ifndef FS_HELPERS_H
#define FS_HELPERS_H

#include <stdint.h>
#include "fat_routines.h"

////////////////////////////////////////////////////////////////////////////////
//                                 GLOBALS                                    //
////////////////////////////////////////////////////////////////////////////////

/*
 * Globals to track metadata about the current mounted filesystem
 */
extern int fs_fd; // reference to file system that is currently mounted
extern int block_size; // the size of each block in the filesystem
extern int num_fat_blocks; // number of blocks in the FAT region
extern int fat_size; // size of the FAT region in bytes
extern uint16_t* fat; // pointer to the FAT region in memory, for efficient access
extern bool is_mounted; // indicator for whether any filesystem is mounted
extern int MAX_FDS;
extern fd_entry_t fd_table[100];  // file descriptor table

////////////////////////////////////////////////////////////////////////////////
//                            FD TABLE HELPERS                                //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Initializes all entries in the file descriptor table to not in use
 *
 * @param fd_table pointer to the file descriptor table to initialize
 */
void init_fd_table(fd_entry_t* fd_table);

/**
 * @brief Finds the first available file descriptor in the table
 *
 * @param fd_table pointer to the file descriptor table to search
 * @return index of the first free file descriptor, or -1 if none available
 */
int get_free_fd(fd_entry_t* fd_table);


/**
 * @brief Increments the reference count of a file descriptor.
 *
 * @param fd file descriptor to increment
 * @return new reference count, or -1 on error
 */
int increment_fd_ref_count(int fd);

/**
 * @brief Decrements the reference count of a file descriptor.
 *
 * @param fd file descriptor to decrement
 * @return new reference count, or -1 on error
 */
int decrement_fd_ref_count(int fd);

/**
 * @brief Checks if a file has executable permissions in the PennFAT filesystem.
 *
 * @param fd The fd of the file to check.
 * @return 1 if the file has executable permissions, 0 if it doesn't, -1 if an error occurred.
 */
int has_executable_permission(int fd);

/**
 * @brief Allocates a free block in the FAT
 *
 * @return block number of the allocated block, or 0 if no free blocks available
 */
uint16_t allocate_block();

/**
 * @brief Searches for a file in the root directory
 *
 * @param filename name of the file to find
 * @param entry pointer to store the directory entry if found
 * @return offset of the entry in the directory if found, -1 if not found
 */
int find_file(const char* filename, dir_entry_t* entry);

/**
 * @brief Adds a new file entry to the root directory
 *
 * @param filename name of the file to add
 * @param size size of the file in bytes
 * @param first_block block number of the first block of the file
 * @param type file type (regular, directory, etc.)
 * @param perm file permissions
 * @return offset of the new entry in the directory if successful, -1 on error
 */
int add_file_entry(const char* filename,
                   uint32_t size,
                   uint16_t first_block,
                   uint8_t type,
                   uint8_t perm);

/**
 * @brief Marks a file entry as deleted and frees its blocks in the FAT.
 *
 * This function takes a directory entry and its offset in the directory,
 * marks it as deleted in the directory, and frees all blocks in its FAT chain.
 *
 * @param entry the entry struct of the file to mark as deleted.
 * @param offset the offset of the entry in the directory
 * @returns 0 on success, -1 on error
 */
int mark_entry_as_deleted(dir_entry_t* entry, int offset);

////////////////////////////////////////////////////////////////////////////////
//                                CP HELPERS                                  //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Copies a file from the host OS to the PennFAT filesystem
 *
 * @param host_filename path to the file on the host OS
 * @param pennfat_filename name to give the file in PennFAT
 * @return 0 on success, -1 on error
 */
int copy_host_to_pennfat(const char* host_filename, const char* pennfat_filename);

/**
 * @brief Copies a file from the PennFAT filesystem to the host OS
 *
 * @param pennfat_filename name of the file in PennFAT
 * @param host_filename path to save the file on the host OS
 * @return 0 on success, -1 on error
 */
int copy_pennfat_to_host(const char* pennfat_filename, const char* host_filename);

/**
 * @brief Copies a file from a source file to a destination file
 *
 * @param source_filename name of the source filename
 * @param dest_filename name of the destination filename
 * @return 0 on success, -1 on error
 */
int copy_source_to_dest(const char* source_filename, const char* dest_filename);

////////////////////////////////////////////////////////////////////////////////
//                                EXTRA CREDIT                                //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Compacts the root directory by removing all deleted entries
 * 
 * @return 0 on success, -1 on error
 */
int compact_directory();


#endif
