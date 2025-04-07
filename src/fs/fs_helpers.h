#ifndef FS_HELPERS_H
#define FS_HELPERS_H

////////////////////////////////////////////////////////////////////////////////
//                          KERNEL FS HELPERS                                 //
////////////////////////////////////////////////////////////////////////////////

// add comments later

int find_free_fd(void);
int find_file_metadata(const char *fname);
int allocate_new_file(const char *fname, uint8_t permissions);
void* get_block_data(uint16_t block_num);
uint16_t follow_fat_chain(uint16_t start_block, uint32_t offset, uint32_t *block_offset);
int read_file_metadata(int meta_idx, file_metadata_t *metadata);
int update_file_metadata(int meta_idx, file_metadata_t *metadata);
int get_metadata_count(void);

#endif