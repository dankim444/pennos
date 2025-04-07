#include "fs_helpers.h"
#include "fat.h"
#include "../lib/pennos-errno.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>

int find_free_fd(void) {
    for (int i = 0; i < MAX_GLOBAL_FD; i++) {
        if (!sys_fd_table[i].in_use) {
            return i;
        }
    }
    return -1;
}

int find_file_metadata(const char *fname) {
    int total_files = get_metadata_count();
    
    for (int i = 0; i < total_files; i++) {
        file_metadata_t metadata;
        if (read_file_metadata(i, &metadata) >= 0 && 
            metadata.filename[0] != '\0' &&
            strcmp(metadata.filename, fname) == 0) {
            return i;
        }
    }
    
    return -1;
}

int allocate_new_file(const char *fname, uint8_t permissions) {
    int total_files = get_metadata_count();
    int meta_idx = -1;
    
    for (int i = 0; i < total_files; i++) {
        file_metadata_t metadata;
        if (read_file_metadata(i, &metadata) >= 0 && metadata.filename[0] == '\0') {
            meta_idx = i;
            break;
        }
    }
    
    if (meta_idx < 0) {
        return -1;
    }
    file_metadata_t metadata;
    strncpy(metadata.filename, fname, sizeof(metadata.filename) - 1);
    metadata.filename[sizeof(metadata.filename) - 1] = '\0';
    metadata.first_block = FAT_FREE;
    metadata.size = 0;
    metadata.permissions = permissions;
    metadata.timestamp = time(NULL);
    if (update_file_metadata(meta_idx, &metadata) < 0) {
        return -1;
    }
    
    return meta_idx;
}

void* get_block_data(uint16_t block_num) {
    int data_offset = fat_get_data_offset();
    int block_size = fat_get_block_size();
    static char *mapped_file = NULL;
    
    if (mapped_file == NULL) {
        return NULL;
    }
    
    return mapped_file + data_offset + (block_num * block_size);
}

uint16_t follow_fat_chain(uint16_t start_block, uint32_t offset, uint32_t *block_offset) {
    int block_size = fat_get_block_size();
    uint32_t blocks_to_skip = offset / block_size;
    
    uint16_t current_block = start_block;
    for (uint32_t i = 0; i < blocks_to_skip && current_block != FAT_EOF; i++) {
        current_block = fat_get_block(current_block);
    }
    
    if (block_offset) {
        *block_offset = offset % block_size;
    }
    
    return current_block;
}

int read_file_metadata(int meta_idx, file_metadata_t *metadata) {
    // TODO
    return -1;
}

int update_file_metadata(int meta_idx, file_metadata_t *metadata) {
    // TODO
    return 0;
}

int get_metadata_count(void) {
    // TODO
    return 1024; // arbitrary for now
}