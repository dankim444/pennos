#ifndef FS_SYS_CALLS_H_
#define FS_SYS_CALLS_H_

#include <stddef.h>

#define F_WRITE  1 // TODO-->check if these numbers are good, I just used them b/c bitwise ops
#define F_READ   2
#define F_APPEND 4

#define STDIN_FILENO 0 // TODO --> check if we want this
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

int s_open(const char *fname, int mode);
int s_read(int fd, int n, char *buf);
int s_write(int fd, const char *str, int n);
int s_close(int fd);
int s_unlink(const char *fname);
int s_lseek(int fd, int offset, int whence);
int s_ls(const char *filename);


#endif
