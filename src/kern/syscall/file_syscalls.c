#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <copyinout.h>
#include <current.h>
#include <proc.h>
#include <filetable.h>
#include <syscall.h>
#include <kern/iovec.h>
#include <limits.h>
#include <uio.h>
#include <vfs.h>

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
sys_lseek(int fd, off_t pos, int code, off_t *retval){
	return fdtable_lseek(curproc->p_fdtable, fd, pos, code, retval);
}

int
sys_dup2(int oldfd, int newfd, int32_t *retval){
	return fdtable_dup2(curproc->p_fdtable, oldfd, newfd, retval);
}

int
sys_chdir(userptr_t path)
{
	char *kpath;
	int result;

	kpath = kmalloc(PATH_MAX);
	if (kpath == NULL) {
		return ENOMEM;
	}

	result = copyinstr(path, kpath, PATH_MAX, NULL);
	if (result) {
		kfree(kpath);
		return result;
	}

	result = vfs_chdir(kpath);
	kfree(kpath);
	return result;
}

int
sys___getcwd(userptr_t buf, size_t buflen, int32_t *retval)
{
	struct iovec iov;
	struct uio u;
	int result;

	iov.iov_ubase = buf;
	iov.iov_len = buflen;

	u.uio_iov = &iov;
	u.uio_iovcnt = 1;
	u.uio_offset = 0;
	u.uio_resid = buflen;
	u.uio_segflg = UIO_USERSPACE;
	u.uio_rw = UIO_READ;
	u.uio_space = proc_getas();

	result = vfs_getcwd(&u);
	if (result) {
		return result;
	}

	*retval = buflen - u.uio_resid;
	return 0;
}