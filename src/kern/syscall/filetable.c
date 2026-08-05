#include "opt-shell.h"
#if OPT_SHELL

#include <types.h>
#include <kern/errno.h>
#include <kern/limits.h>
#include <lib.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <synch.h>
#include <copyinout.h>
#include <filetable.h>

#define SYSTEM_OPEN_MAX (10 * OPEN_MAX)

struct open_file {
	struct vnode *of_vn;
	off_t of_offset;
	int of_flags;
	unsigned int of_refcount;
	struct lock *of_lock;
};

struct fd_table {
	struct open_file *ft_entries[OPEN_MAX];
	struct vnode *ft_cwd;
	struct lock *ft_lock;
};

static struct open_file *system_table[SYSTEM_OPEN_MAX];
static struct lock *system_table_lock;   /* protegge la ricerca slot liberi */

#endif