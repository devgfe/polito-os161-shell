#include "opt-shell.h"
#if OPT_SHELL

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <copyinout.h>
#include <current.h>
#include <proc.h>
#include <filetable.h>
#include <syscall.h>

int
sys_read(int fd, userptr_t buf, size_t size, int32_t *retval)
{
	void *kbuf;
	int result;

	if (size == 0) {
		*retval = 0;
		return 0;
	}

	kbuf = kmalloc(size);
	if (kbuf == NULL) {
		return ENOMEM;
	}

	result = fdtable_read(curproc->p_fdtable, fd, kbuf, size, retval);
	if (result) {
		kfree(kbuf);
		return result;
	}

	result = copyout(kbuf, buf, *retval);
	kfree(kbuf);
	return result;
}

int
sys_write(int fd, userptr_t buf, size_t size, int32_t *retval)
{
	void *kbuf;
	int result;

	if (size == 0) {
		*retval = 0;
		return 0;
	}

	kbuf = kmalloc(size);
	if (kbuf == NULL) {
		return ENOMEM;
	}

	result = copyin(buf, kbuf, size);
	if (result) {
		kfree(kbuf);
		return result;
	}

	result = fdtable_write(curproc->p_fdtable, fd, kbuf, size, retval);
	kfree(kbuf);
	return result;
}

int
sys_lseek(int fd, off_t pos, int code, off_t *retval)
{
	return fdtable_lseek(curproc->p_fdtable, fd, pos, code, retval);
}

#endif /* OPT_SHELL */