#ifndef FAT_ROUTINES_H
#define FAT_ROUTINES_H

////////////////////////////////////////////////////////////////////////////////
//                           SPECIAL ROUTINES                                 //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Creates a PennFAT filesystem in the file named fs_name.
 * 
 * This function initializes a new PennFAT filesystem with the specified parameters.
 * The number of blocks in the FAT ranges from 1 through 32, and the block size
 * is determined by block_size_config (0=256B, 1=512B, 2=1024B, 3=2048B, 4=4096B).
 * 
 * @param fs_name The name of the file to create the filesystem in.
 * @param blocks_in_fat The number of blocks in the FAT (1-32).
 * @param block_size_config The block size configuration (0-4).
 */
void mkfs(const char *fs_name, int blocks_in_fat, int block_size_config);

/**
 * @brief Mounts the filesystem named fs_name by loading its FAT into memory.
 * 
 * This function loads the filesystem's FAT into memory for subsequent operations.
 * Only one filesystem can be mounted at a time.
 * 
 * @param fs_name The name of the filesystem file to mount.
 * @return 0 on success, -1 on failure with P_ERRNO set.
 */
int mount(const char *fs_name);

/**
 * @brief Unmounts the currently mounted filesystem.
 * 
 * This function flushes any pending changes and unmounts the filesystem.
 * 
 * @return 0 on success, -1 on failure with P_ERRNO set.
 */
int unmount();

////////////////////////////////////////////////////////////////////////////////
//                           OTHER ROUTINES                                   //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Concatenates and displays files.
 * 
 * This function reads the content of files and writes it to stdout or to another file.
 * It supports reading from stdin when no input files are specified.
 * 
 * Usage formats:
 * - cat FILE ... (displays content to stdout)
 * - cat FILE ... -w OUTPUT_FILE (writes content to OUTPUT_FILE, overwriting)
 * - cat FILE ... -a OUTPUT_FILE (appends content to OUTPUT_FILE)
 * - cat -w OUTPUT_FILE (reads from stdin, writes to OUTPUT_FILE)
 * - cat -a OUTPUT_FILE (reads from stdin, appends to OUTPUT_FILE)
 * 
 * @param arg Arguments array (command line arguments)
 * @return void pointer (unused)
 */
void* cat(void *arg);

/**
 * @brief Lists files in the current directory.
 * 
 * This function displays information about files in the current directory,
 * including block number, permissions, size, and name.
 * 
 * @param arg Arguments array (command line arguments)
 * @return void pointer (unused)
 */
void* ls(void *arg);

/**
 * @brief Creates empty files or updates timestamps.
 * 
 * For each file argument, creates the file if it doesn't exist,
 * or updates its timestamp if it already exists.
 * 
 * @param arg Arguments array (command line arguments)
 * @return void pointer (unused)
 */
void* touch(void *arg);

/**
 * @brief Renames files.
 * 
 * Renames the source file to the destination name.
 * If the destination file already exists, it will be overwritten.
 * 
 * Usage: mv SOURCE DEST
 * 
 * @param arg Arguments array (command line arguments)
 * @return void pointer (unused)
 */
void* mv(void *arg);

/**
 * @brief Copies files.
 * 
 * Copies the source file to the destination.
 * If the destination file already exists, it will be overwritten.
 * 
 * Usage formats:
 * - cp SOURCE DEST (copies within PennFAT)
 * - cp -h SOURCE DEST (copies from host OS to PennFAT)
 * - cp SOURCE -h DEST (copies from PennFAT to host OS)
 * 
 * @param arg Arguments array (command line arguments)
 * @return void pointer (unused)
 */
void* cp(void *arg);

/**
 * @brief Removes files.
 * 
 * Deletes one or more files from the filesystem.
 * Each file is processed as a separate transaction.
 * 
 * @param arg Arguments array (command line arguments)
 * @return void pointer (unused)
 */
void* rm(void *arg);

/**
 * @brief Changes file permissions.
 * 
 * Modifies the permissions of the specified file.
 * 
 * Usage formats:
 * - chmod +x FILE (adds executable permission)
 * - chmod +rw FILE (adds read and write permissions)
 * - chmod -wx FILE (removes write and executable permissions)
 * 
 * @param arg Arguments array (command line arguments)
 * @return void pointer (unused)
 */
void* chmod(void *arg);

#endif