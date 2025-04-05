#include "fs_syscalls.h"

int s_open(const char *fname, int mode) {
    return -1; // TODO --> implement s_open
}

int s_read(int fd, int n, char *buf) {
    return -1; // TODO --> implement s_read
}

int s_write(int fd, const char *str, int n) {
    return -1; // TODO --> implement s_write
}

int s_close(int fd) {
    return -1; // TODO --> implement s_close
}

int s_unlink(const char *fname) {
    return -1; // TODO --> implement s_unlink
}

int s_lseek(int fd, int offset, int whence) {
    return -1; // TODO --> implement s_lseek
}

int s_ls(const char *filename) {
    return -1; // TODO --> implement s_ls
}