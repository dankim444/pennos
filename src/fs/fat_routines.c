/**
* Creates a PennFAT filesystem in the file named fs_name
*/
void mkfs(const char *fs_name, int blocks_in_fat, int block_size_config) {
    return;
}
int mount(const char *fs_name) {
    return 0;
}
int unmount() {
    return 0;
}

void* cat(void *arg) {
    return;
}
void* ls(void *arg);
void* touch(void *arg);
void* mv(void *arg);
void* cp(void *arg);
void* rm(void *arg);
void* chmod(void *arg);