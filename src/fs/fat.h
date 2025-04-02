#ifndef FS_FAT_H_
#define FS_FAT_H_

#include <stdint.h>
#include <stdbool.h>

#define FAT_EOF 0xFFFF
#define FAT_FREE 0x0000

// FAT block management
int fat_init(const char *fs_name);
void fat_unmount();
uint16_t fat_get_block(uint16_t index);
void fat_set_block(uint16_t index, uint16_t value);
int fat_find_free_block();
int fat_save();
int fat_load(const char *fs_name);
int fat_get_block_size();
int fat_get_data_offset();
int fat_get_num_entries();
int fat_get_fat_size_bytes();

#endif
