#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include "opt-shell.h"

#if OPT_SHELL
struct fd_table;

/* Called only when the system starts, before any process is created. */
void filetable_bootstrap(void);

/* Create a file descriptor table with stdin, stdout, and stderr attached to the console. */
struct fd_table *fdtable_create_standard(void);

/* Destroy a file descriptor table, releasing every open file it still holds. */
void fdtable_destroy(struct fd_table *table);

/* Open the given path assigning a file descriptor in the table. */
int fdtable_open(struct fd_table *table, const char *kpath, int flags, mode_t mode, int *fd_ret);

/* Close a file descriptor. */ 
int fdtable_close(struct fd_table *table, int fd);

/* Read data from an open file descriptor into a kernel buffer. */
int fdtable_read(struct fd_table *table, int fd, void *kbuf, size_t size, int32_t *retval);

/* Write data from a kernel buffer to an open file descriptor. */
int fdtable_write(struct fd_table *table, int fd, const void *kbuf, size_t size, int32_t *retval);

/* Move the file offset of a descriptor and return the new position. */
int fdtable_lseek(struct fd_table *table, int fd, off_t pos, int code, off_t *retval);

/* Make newfd refer to the same open file as oldfd. */
int fdtable_dup2(struct fd_table *table, int oldfd, int newfd, int32_t *retval);

/* Create a copy of a file descriptor table sharing its open files with the original. */
int fdtable_clone(struct fd_table *source, struct fd_table **copy);
#endif

#endif /* _FILETABLE_H_ */