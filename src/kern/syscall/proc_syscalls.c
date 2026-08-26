#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <limits.h>
#include <lib.h>
#include <addrspace.h>
#include <copyinout.h>
#include <proc.h>
#include <vfs.h>
#include <syscall.h>
#include <clock.h>
#include <thread.h>
#include <current.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <synch.h>
#include <machine/trapframe.h>
#include <initstack.h>
#include <filetable.h>

/*
 * sys_execv - replace current process image with a new program
 *
 * Flow: 
 * 		-> copy program path and args from userspace 
 * 		-> open and load ELF
 *      -> create new address space 
 * 		-> build stack with argv 
 * 		-> enter new process
 *
 * On failure, restores the old address space and returns errno.
 */
int 
sys_execv(const_userptr_t user_program, const_userptr_t user_argv) 
{
	struct vnode *v;
	struct addrspace *newas;
	struct addrspace *oldas;
	char *kprogram;
	char *kargs;
	char **kargv;
	userptr_t new_user_argv;
	vaddr_t entrypoint;
	vaddr_t stackptr;
	size_t argsbytes;
	int argc;
	int result;
	int switched;

	v = NULL;
	newas = NULL;
	oldas = NULL;
	kprogram = NULL;
	kargs = NULL;
	kargv = NULL;
	switched = 0;

	/* Copy program path from user space */
	kprogram = kmalloc(PATH_MAX);
	if (kprogram == NULL) {
		result = ENOMEM;
		goto fail;
	}

	result = copyinstr(user_program, kprogram, PATH_MAX, NULL);
	if (result) {
		goto fail;
	}

	/* Copy all argv strings into a flat kernel buffer */
	kargs = kmalloc(ARG_MAX);
	if (kargs == NULL) {
		result = ENOMEM;
		goto fail;
	}

	kargv = kmalloc(ARG_MAX);
	if (kargv == NULL) {
		result = ENOMEM;
		goto fail;
	}

	argsbytes = 0;
	argc = 0;
	while (true) {
		userptr_t user_argaddr;

		result = copyin(
			(const_userptr_t)((vaddr_t)user_argv + argc * sizeof(userptr_t)),
			&user_argaddr,
			sizeof(user_argaddr));
		if (result) {
			goto fail;
		}
		if (user_argaddr == NULL) {
			break;
		}

		size_t reserved_bytes = argsbytes + (argc + 2) * sizeof(vaddr_t); // The +2 accounts for: new arg pointer + NULL terminator
		if (reserved_bytes >= ARG_MAX) {
			result = E2BIG;
			goto fail;
		}

		kargv[argc] = kargs + argsbytes;
		
		size_t available = ARG_MAX - reserved_bytes;
		size_t single_arg_bytes;

		result = copyinstr(
			(const_userptr_t)user_argaddr,
			kargv[argc], 
			available, 
			&single_arg_bytes
		);
		if (result) {
			if (result == ENAMETOOLONG) result = E2BIG;
			goto fail;
		}

		argsbytes += single_arg_bytes;
		argc++;
	}
	kargv[argc] = NULL;

	result = vfs_open(kprogram, O_RDONLY, 0, &v);
	if (result) {
		goto fail;
	}

	newas = as_create();
	if (newas == NULL) {
		result = ENOMEM;
		goto fail;
	}

	oldas = proc_setas(newas);
	switched = 1;
	as_activate();

	result = load_elf(v, &entrypoint);
	vfs_close(v);
	v = NULL;
	if (result) {
		goto fail;
	}

	result = as_define_stack(newas, &stackptr);
	if (result) {
		goto fail;
	}

	/* Build the initial user stack layout with argv strings and
	 * the argv vector; the returned argv doubles as stack pointer. */
	result = initstack(kargv, argc, stackptr, &new_user_argv);
	if (result) {
		goto fail;
	}

	if (oldas != NULL) {
		as_destroy(oldas);
	}
	kfree(kargv);
	kfree(kargs);
	kfree(kprogram);

	/* Pass control to new process - does not return on success. */
	enter_new_process(argc, new_user_argv, NULL, (vaddr_t)new_user_argv, entrypoint);

	panic("enter_new_process returned\n");
	return EINVAL;

fail:
	if (v != NULL) {
		vfs_close(v);
	}
	if (switched) { /* Restore old address space if we already switched to the new one */
		proc_setas(oldas);
		as_activate();
	}
	if (newas != NULL) {
		as_destroy(newas);
	}
	if (kargv != NULL) {
		kfree(kargv);
	}
	if (kargs != NULL) {
		kfree(kargs);
	}
	if (kprogram != NULL) {
		kfree(kprogram);
	}
	return result;
}


void sys__exit(int exitcode)
{
	struct proc *p = curproc;

	lock_acquire(p->p_waitlock);

	p->p_exitcode = _MKWAIT_EXIT(exitcode);

	lock_release(p->p_waitlock);

	thread_exit();
	panic("thread_exit returned\n");
}


int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval)
{
	/*
	* OS/161 currently supports only options == 0.
	* The parameter is kept for compatibility with the standard POSIX waitpid interface,
	* where additional flags (e.g., WNOHANG) may be supported in the future.
	*/
	if (options != 0) {
		return EINVAL;
	}

	struct proc *child = proc_lookup(pid);

	if (child == NULL) {
		return ESRCH;
	}

	if (child->p_parent != curproc->p_pid) {
		return ECHILD;
	}

	int code = proc_wait(child);

	int err = copyout(&code, status, sizeof(int));
	if (err) {
		return err;
	}

	proc_destroy(child);

	*retval = pid;
	return 0;
}


int sys_getpid(pid_t *retval){
	*retval = curproc->p_pid;
	return 0;
}


int sys_fork(struct trapframe *tf, pid_t *retval){
	int err;
	struct proc *child;
	struct addrspace *child_as;
	struct trapframe *child_tf;

	child_tf = kmalloc(sizeof(struct trapframe));
	if (child_tf == NULL) {
		return ENOMEM;
	}

	*child_tf = *tf;

	child = proc_create_runprogram(curproc->p_name);
	if (child == NULL) {
		kfree(child_tf);
		return ENOMEM;
	}
	// Miglioramento possibile, da discutere: implementare la copy on write
	err = as_copy(curproc->p_addrspace, &child_as);
	if (err) {
		proc_destroy(child);
		kfree(child_tf);
		return err;
	}

	child->p_addrspace = child_as;

	struct fd_table *child_fdtable;

	err = fdtable_clone(curproc->p_fdtable, &child_fdtable);
	if (err) {
		proc_destroy(child);
		kfree(child_tf);
		return err;
	}
	fdtable_destroy(child->p_fdtable);
	child->p_fdtable = child_fdtable;

	err = thread_fork(
		curthread->t_name,
		child,
		enter_forked_process, 
		child_tf,
		0
	);

	if (err) {
		proc_destroy(child);
		kfree(child_tf);
		return err;
	}

	*retval = child->p_pid;
	return 0;
}