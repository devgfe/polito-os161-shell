#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include "opt-shell.h"

#if OPT_SHELL
struct fd_table;

void filetable_bootstrap(void);
struct fd_table *fdtable_create_standard(void);
void fdtable_destroy(struct fd_table *table);
int fdtable_open(struct fd_table *table, const char *kpath, int flags, mode_t mode, int *fd_ret);
int fdtable_close(struct fd_table *table, int fd);
int fdtable_read(struct fd_table *table, int fd, void *kbuf, size_t size, int32_t *retval);
int fdtable_write(struct fd_table *table, int fd, const void *kbuf, size_t size, int32_t *retval);
#endif

#endif /* _FILETABLE_H_ */