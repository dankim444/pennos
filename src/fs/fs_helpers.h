#ifndef FS_HELPERS_H
#define FS_HELPERS_H

#include <stdint.h>  // For uint8_t, uint16_t, uint32_t types

////////////////////////////////////////////////////////////////////////////////
//                          KERNEL FS HELPERS                                 //
////////////////////////////////////////////////////////////////////////////////

// Forward declaration of file_metadata_t structure
typedef struct file_metadata_st file_metadata_t;

// Structure for file metadata
struct file_metadata_st {
    char filename[32];         // Null-terminated filename
    uint16_t first_block;      // First block of file data
    uint32_t size;             // File size in bytes
    uint8_t permissions;       // File permissions (read/write/execute)
    uint32_t timestamp;        // Last modification time
    uint8_t is_used;           // 1 if entry is used, 0 if free
};

// Maximum number of file descriptors
#define MAX_GLOBAL_FD 64

// File descriptor table entry structure
typedef struct {
    int in_use;               // Whether this fd is in use
    int meta_idx;             // Index into metadata table
    uint32_t offset;          // Current file position
    int flags;                // Open mode flags
} sys_fd_entry_t;

// Global file descriptor table
extern sys_fd_entry_t sys_fd_table[MAX_GLOBAL_FD];

/**
 * Find an unused file descriptor in the system file descriptor table.
 * 
 * @return Index of free fd on success, -1 if no free fd available
 */
int find_free_fd(void);

/**
 * Find a file's metadata entry by name.
 * 
 * @param fname Filename to search for
 * @return Metadata index on success, -1 if file not found
 */
int find_file_metadata(const char *fname);

/**
 * Allocate a new file in the metadata table.
 * 
 * @param fname Filename for the new file
 * @param permissions Initial file permissions
 * @return Metadata index on success, -1 on error
 */
int allocate_new_file(const char *fname, uint8_t permissions);

/**
 * Get a pointer to a block's data.
 * 
 * @param block_num Block number to access
 * @return Pointer to block data, or NULL on error
 */
void* get_block_data(uint16_t block_num);

/**
 * Follow FAT chain to find block containing a specific offset.
 * 
 * @param start_block First block in the chain
 * @param offset Byte offset into the file
 * @param block_offset Output parameter for offset within found block
 * @return Block number containing the offset, or FAT_EOF on error
 */
uint16_t follow_fat_chain(uint16_t start_block, uint32_t offset, uint32_t *block_offset);

/**
 * Read file metadata from the metadata table.
 * 
 * @param meta_idx Index in metadata table
 * @param metadata Output parameter for file metadata
 * @return 0 on success, -1 on error
 */
int read_file_metadata(int meta_idx, file_metadata_t *metadata);

/**
 * Update file metadata in the metadata table.
 * 
 * @param meta_idx Index in metadata table
 * @param metadata New file metadata
 * @return 0 on success, -1 on error
 */
int update_file_metadata(int meta_idx, file_metadata_t *metadata);

/**
 * Get the number of entries in the metadata table.
 * 
 * @return Number of metadata entries
 */
int get_metadata_count(void);

#endif