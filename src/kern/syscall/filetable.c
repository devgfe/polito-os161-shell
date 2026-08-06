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
#include <uio.h>
#include <kern/seek.h>
#include <kern/stat.h>
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

struct fd_table *
fdtable_create_standard(void)
{
	struct fd_table *ft;
	int result;
	int i;
    int fd;

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

	result = fdtable_open(ft, "con:", O_RDONLY, 0, &fd);
	if (result) {
		goto fail;
	}
	result = fdtable_open(ft, "con:", O_WRONLY, 0, &fd);
	if (result) {
		goto fail;
	}
	result = fdtable_open(ft, "con:", O_WRONLY, 0, &fd);
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

int
fdtable_open(struct fd_table *table, const char *kpath, int flags, mode_t mode, int *fd_ret)
{
	struct vnode *vn;
	struct open_file *of;
	char *path_copy;
	int fd;
	int result;

	path_copy = kstrdup(kpath);
	if (path_copy == NULL) {
		return ENOMEM;
	}

	result = vfs_open(path_copy, flags, mode, &vn);
	kfree(path_copy);
	if (result) {
		return result;
	}

	of = system_table_alloc(vn, flags);
	if (of == NULL) {
		vfs_close(vn);
		return ENFILE;
	}

	lock_acquire(table->ft_lock);
	for (fd = 0; fd < OPEN_MAX; fd++) {
		if (table->ft_entries[fd] == NULL) {
			table->ft_entries[fd] = of;
			break;
		}
	}
	lock_release(table->ft_lock);

	if (fd == OPEN_MAX) {
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
		return EMFILE;
	}

	*fd_ret = fd;
	return 0;
}

int
fdtable_close(struct fd_table *table, int fd)
{
	struct open_file *of;

	if (fd < 0 || fd >= OPEN_MAX) {
		return EBADF;
	}

	lock_acquire(table->ft_lock);
	of = table->ft_entries[fd];
	if (of == NULL) {
		lock_release(table->ft_lock);
		return EBADF;
	}
	table->ft_entries[fd] = NULL;
	lock_release(table->ft_lock);

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

	return 0;
}

int
fdtable_read(struct fd_table *table, int fd, void *kbuf, size_t size, int32_t *retval)
{
	struct open_file *of;
	struct iovec iov;
	struct uio u;
	int result;

	if (fd < 0 || fd >= OPEN_MAX) {
		return EBADF;
	}

	lock_acquire(table->ft_lock);
	of = table->ft_entries[fd];
	lock_release(table->ft_lock);

	if (of == NULL) {
		return EBADF;
	}
	if ((of->of_flags & O_ACCMODE) == O_WRONLY) {
		return EBADF;
	}

	lock_acquire(of->of_lock);
	uio_kinit(&iov, &u, kbuf, size, of->of_offset, UIO_READ);

	result = VOP_READ(of->of_vn, &u);
	if (result) {
		lock_release(of->of_lock);
		return result;
	}

	*retval = size - u.uio_resid;
	of->of_offset = u.uio_offset;
	lock_release(of->of_lock);

	return 0;
}

int
fdtable_write(struct fd_table *table, int fd, const void *kbuf, size_t size, int32_t *retval)
{
	struct open_file *of;
	struct iovec iov;
	struct uio u;
	int result;

	if (fd < 0 || fd >= OPEN_MAX) {
		return EBADF;
	}

	lock_acquire(table->ft_lock);
	of = table->ft_entries[fd];
	lock_release(table->ft_lock);

	if (of == NULL) {
		return EBADF;
	}
	if ((of->of_flags & O_ACCMODE) == O_RDONLY) {
		return EBADF;
	}

	lock_acquire(of->of_lock);
	uio_kinit(&iov, &u, (void *)kbuf, size, of->of_offset, UIO_WRITE);

	result = VOP_WRITE(of->of_vn, &u);
	if (result) {
		lock_release(of->of_lock);
		return result;
	}

	*retval = size - u.uio_resid;
	of->of_offset = u.uio_offset;
	lock_release(of->of_lock);

	return 0;
}

int
fdtable_lseek(struct fd_table *table, int fd, off_t pos, int code, off_t *retval)
{
	struct open_file *of;
	struct stat statbuf;
	off_t new_offset;
	int result;

	if (fd < 0 || fd >= OPEN_MAX) {
		return EBADF;
	}

	lock_acquire(table->ft_lock);
	of = table->ft_entries[fd];
	lock_release(table->ft_lock);

	if (of == NULL) {
		return EBADF;
	}

	if (!VOP_ISSEEKABLE(of->of_vn)) {
		return ESPIPE;
	}

	lock_acquire(of->of_lock);

	switch (code) {
	    case SEEK_SET:
		new_offset = pos;
		break;
	    case SEEK_CUR:
		new_offset = of->of_offset + pos;
		break;
	    case SEEK_END:
		result = VOP_STAT(of->of_vn, &statbuf);
		if (result) {
			lock_release(of->of_lock);
			return result;
		}
		new_offset = statbuf.st_size + pos;
		break;
	    default:
		lock_release(of->of_lock);
		return EINVAL;
	}

	if (new_offset < 0) {
		lock_release(of->of_lock);
		return EINVAL;
	}

	of->of_offset = new_offset;
	*retval = new_offset;

	lock_release(of->of_lock);
	return 0;
}

int
fdtable_dup2(struct fd_table *table, int oldfd, int newfd, int32_t *retval)
{
	struct open_file *of;
	struct open_file *old_at_new;

	if (oldfd < 0 || oldfd >= OPEN_MAX || newfd < 0 || newfd >= OPEN_MAX) {
		return EBADF;
	}

	lock_acquire(table->ft_lock);

	of = table->ft_entries[oldfd];
	if (of == NULL) {
		lock_release(table->ft_lock);
		return EBADF;
	}

	if (oldfd == newfd) {
		lock_release(table->ft_lock);
		*retval = newfd;
		return 0;
	}

	old_at_new = table->ft_entries[newfd];
	table->ft_entries[newfd] = of;

	lock_acquire(system_table_lock);
	of->of_refcount++;
	lock_release(system_table_lock);

	lock_release(table->ft_lock);

	if (old_at_new != NULL) {
		lock_acquire(system_table_lock);
		old_at_new->of_refcount--;
		if (old_at_new->of_refcount == 0) {
			system_table[old_at_new->of_index] = NULL;
			lock_release(system_table_lock);
			vfs_close(old_at_new->of_vn);
			lock_destroy(old_at_new->of_lock);
			kfree(old_at_new);
		} else {
			lock_release(system_table_lock);
		}
	}

	*retval = newfd;
	return 0;
}

#endif /* OPT_SHELL */

#endif