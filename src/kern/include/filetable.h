#ifndef _FILETABLE_H_
#define _FILETABLE_H_

#include "opt-shell.h"

#if OPT_SHELL
struct fd_table;

void filetable_bootstrap(void);
struct fd_table *fdtable_create_standard(void);
void fdtable_destroy(struct fd_table *table);
#endif

#endif /* _FILETABLE_H_ */