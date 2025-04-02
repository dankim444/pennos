#ifndef FS_KFUNCS_H_
#define FS_KFUNCS_H_

#include <stddef.h>

int k_open(const char *fname, int mode);
int k_read(int fd, int n, char *buf);
int k_write(int fd, const char *str, int n);
int k_close(int fd);
int k_unlink(const char *fname);
int k_lseek(int fd, int offset, int whence);
int k_ls(const char *filename);

#endif
