#ifndef FS_SYS_CALLS_H_
#define FS_SYS_CALLS_H_

#include <stddef.h>

int s_open(const char *fname, int mode);
int s_read(int fd, int n, char *buf);
int s_write(int fd, const char *str, int n);
int s_close(int fd);
int s_unlink(const char *fname);
int s_lseek(int fd, int offset, int whence);
int s_ls(const char *filename);

#endif
