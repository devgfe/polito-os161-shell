#include "opt-shell.h"
#if OPT_SHELL

#include <types.h>
#include <kern/errno.h>
#include <limits.h>
#include <kern/fcntl.h>
#include <kern/unistd.h>
#include <lib.h>
#include <vnode.h>
#include <vfs.h>
#include <synch.h>
#include <filetable.h>
#include "opt-shell.h"

#if OPT_SHELL

#define SYSTEM_OPEN_MAX (10 * OPEN_MAX)

struct open_file {
	struct vnode *of_vn;
	off_t of_offset;
	int of_flags;
	unsigned int of_refcount;
	struct lock *of_lock;
	int of_index;		/* slot in system_table, per poterlo liberare */
};

struct fd_table {
	struct open_file *ft_entries[OPEN_MAX];
	struct vnode *ft_cwd;
	struct lock *ft_lock;
};

static struct open_file *system_table[SYSTEM_OPEN_MAX];
static struct lock *system_table_lock;

/* da chiamare una volta sola all'avvio del kernel, prima di creare processi */
void
filetable_bootstrap(void)
{
	int i;

	system_table_lock = lock_create("system_open_file_table");
	KASSERT(system_table_lock != NULL);
	for (i = 0; i < SYSTEM_OPEN_MAX; i++) {
		system_table[i] = NULL;
	}
}

static struct open_file *
system_table_alloc(struct vnode *vn, int flags)
{
	struct open_file *of;
	int i;

	of = kmalloc(sizeof(*of));
	if (of == NULL) {
		return NULL;
	}
	of->of_lock = lock_create("open_file");
	if (of->of_lock == NULL) {
		kfree(of);
		return NULL;
	}
	of->of_vn = vn;
	of->of_offset = 0;
	of->of_flags = flags;
	of->of_refcount = 1;

	lock_acquire(system_table_lock);
	for (i = 0; i < SYSTEM_OPEN_MAX; i++) {
		if (system_table[i] == NULL) {
			system_table[i] = of;
			of->of_index = i;
			break;
		}
	}
	lock_release(system_table_lock);

	if (i == SYSTEM_OPEN_MAX) {
		lock_destroy(of->of_lock);
		kfree(of);
		return NULL;
	}

	return of;
}

/* apre path e lo piazza sull'fd indicato; usata per stdin/stdout/stderr, e in futuro da sys_open */
static int
fdtable_open_std(struct fd_table *ft, const char *path, int flags, int fd)
{
	struct vnode *vn;
	struct open_file *of;
	char *kpath;
	int result;

	KASSERT(fd >= 0 && fd < OPEN_MAX);
	KASSERT(ft->ft_entries[fd] == NULL);

	kpath = kstrdup(path);
	if (kpath == NULL) {
		return ENOMEM;
	}

	result = vfs_open(kpath, flags, 0, &vn);
	kfree(kpath);
	if (result) {
		return result;
	}

	of = system_table_alloc(vn, flags);
	if (of == NULL) {
		vfs_close(vn);
		return ENFILE;
	}

	ft->ft_entries[fd] = of;
	return 0;
}

struct fd_table *
fdtable_create_standard(void)
{
	struct fd_table *ft;
	int result;
	int i;

	ft = kmalloc(sizeof(*ft));
	if (ft == NULL) {
		return NULL;
	}

	ft->ft_lock = lock_create("fdtable");
	if (ft->ft_lock == NULL) {
		kfree(ft);
		return NULL;
	}

	for (i = 0; i < OPEN_MAX; i++) {
		ft->ft_entries[i] = NULL;
	}
	ft->ft_cwd = NULL;

	result = fdtable_open_std(ft, "con:", O_RDONLY, STDIN_FILENO);
	if (result) {
		goto fail;
	}
	result = fdtable_open_std(ft, "con:", O_WRONLY, STDOUT_FILENO);
	if (result) {
		goto fail;
	}
	result = fdtable_open_std(ft, "con:", O_WRONLY, STDERR_FILENO);
	if (result) {
		goto fail;
	}

	return ft;

fail:
	fdtable_destroy(ft);
	return NULL;
}

void
fdtable_destroy(struct fd_table *table)
{
	int i;
	struct open_file *of;

	if (table == NULL) {
		return;
	}

	for (i = 0; i < OPEN_MAX; i++) {
		of = table->ft_entries[i];
		if (of == NULL) {
			continue;
		}
		table->ft_entries[i] = NULL;

		lock_acquire(system_table_lock);
		of->of_refcount--;
		if (of->of_refcount == 0) {
			system_table[of->of_index] = NULL;
			lock_release(system_table_lock);
			vfs_close(of->of_vn);
			lock_destroy(of->of_lock);
			kfree(of);
		} else {
			lock_release(system_table_lock);
		}
	}

	if (table->ft_cwd != NULL) {
		VOP_DECREF(table->ft_cwd);
	}

	lock_destroy(table->ft_lock);
	kfree(table);
}

#endif /* OPT_SHELL */

#endif