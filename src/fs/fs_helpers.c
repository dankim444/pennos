#include "fs_helpers.h"
#include "fat.h"
#include <string.h>
#include <time.h>

// Maximum number of metadata entries
#define MAX_METADATA_ENTRIES 64

// Metadata table
static file_metadata_t metadata_table[MAX_METADATA_ENTRIES];
static int metadata_initialized = 0;

// Initialize the metadata table (only once)
static void initialize_metadata_table() {
    if (!metadata_initialized) {
        memset(metadata_table, 0, sizeof(metadata_table));
        metadata_initialized = 1;
    }
}

int find_free_fd() {
    for (int i = 0; i < MAX_GLOBAL_FD; i++) {
        if (!sys_fd_table[i].in_use) {
            return i;
        }
    }
    return -1; // No free file descriptor found
}

int find_file_metadata(const char *fname) {
    if (!metadata_initialized) {
        initialize_metadata_table();
    }
    
    for (int i = 0; i < MAX_METADATA_ENTRIES; i++) {
        if (metadata_table[i].is_used && 
            strcmp(metadata_table[i].filename, fname) == 0) {
            return i;
        }
    }
    
    return -1; // File not found
}

int allocate_new_file(const char *fname, uint8_t permissions) {
    if (!metadata_initialized) {
        initialize_metadata_table();
    }
    
    // Find a free slot in the metadata table
    int free_slot = -1;
    for (int i = 0; i < MAX_METADATA_ENTRIES; i++) {
        if (!metadata_table[i].is_used) {
            free_slot = i;
            break;
        }
    }
    
    if (free_slot == -1) {
        return -1; // No free slots
    }
    
    // Initialize the file metadata
    strncpy(metadata_table[free_slot].filename, fname, 31);
    metadata_table[free_slot].filename[31] = '\0'; // Ensure null termination
    metadata_table[free_slot].first_block = FAT_FREE;
    metadata_table[free_slot].size = 0;
    metadata_table[free_slot].permissions = permissions;
    metadata_table[free_slot].timestamp = time(NULL);
    metadata_table[free_slot].is_used = 1;
    
    return free_slot;
}

void* get_block_data(uint16_t block_num) {
    if (block_num == FAT_FREE || block_num == FAT_EOF) {
        return NULL;
    }
    
    // This is just a placeholder - you'll need to replace this with
    // the actual mechanism to access the mapped data based on your implementation
    static char dummy_data[4096]; // Dummy buffer for testing
    
    return dummy_data; // Replace with actual block data access mechanism
}

uint16_t follow_fat_chain(uint16_t start_block, uint32_t offset, uint32_t *block_offset) {
    if (start_block == FAT_FREE || start_block == FAT_EOF) {
        if (block_offset) {
            *block_offset = 0;
        }
        return FAT_FREE;
    }
    
    int block_size = fat_get_block_size();
    uint32_t block_count = offset / block_size;
    
    uint16_t current_block = start_block;
    for (uint32_t i = 0; i < block_count; i++) {
        current_block = fat_get_block(current_block);
        if (current_block == FAT_FREE || current_block == FAT_EOF) {
            if (block_offset) {
                *block_offset = 0;
            }
            return FAT_FREE;
        }
    }
    
    if (block_offset) {
        *block_offset = offset % block_size;
    }
    
    return current_block;
}

int read_file_metadata(int meta_idx, file_metadata_t *metadata) {
    if (!metadata_initialized) {
        initialize_metadata_table();
    }
    
    if (meta_idx < 0 || meta_idx >= MAX_METADATA_ENTRIES || !metadata) {
        return -1;
    }
    
    *metadata = metadata_table[meta_idx];
    return 0;
}

int update_file_metadata(int meta_idx, file_metadata_t *metadata) {
    if (!metadata_initialized) {
        initialize_metadata_table();
    }
    
    if (meta_idx < 0 || meta_idx >= MAX_METADATA_ENTRIES || !metadata) {
        return -1;
    }
    
    metadata_table[meta_idx] = *metadata;
    return 0;
}

int get_metadata_count() {
    return MAX_METADATA_ENTRIES;
}