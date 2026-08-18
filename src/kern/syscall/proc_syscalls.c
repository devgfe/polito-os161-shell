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

/*
 * sys_execv - replace current process image with a new program
 *
 * Flow: copy program path and args from userspace -> open and load ELF
 *       -> create new address space -> build stack with argv -> enter new process
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
	vaddr_t entrypoint;
	vaddr_t stackptr;
	vaddr_t argv_vector_base;
	vaddr_t argv_strings_base;
	vaddr_t argv_vector_base_aligned;
	vaddr_t argv_strings_base_aligned;
	vaddr_t argaddr;
	size_t argsbytes;
	size_t argsoffset;
	int argc;
	int result;
	int switched;

	v = NULL;
	newas = NULL;
	oldas = NULL;
	kprogram = NULL;
	kargs = NULL;
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

	/* Copy all argv strings into a flat kernel buffer.
	 * We reserve space for the pointer vector as we go to detect E2BIG early. */
	kargs = kmalloc(ARG_MAX);
	if (kargs == NULL) {
		result = ENOMEM;
		goto fail;
	}

	argsbytes = 0;
	argc = 0;
	while (true) {
		userptr_t user_arg;

		result = copyin(
			(const_userptr_t)((vaddr_t)user_argv + argc * sizeof(userptr_t)),
			&user_arg,
			sizeof(user_arg));
		if (result) {
			goto fail;
		}
		if (user_arg == NULL) {
			break;
		}

		size_t reserved_bytes = argsbytes + (argc + 2) * sizeof(vaddr_t); // The +2 accounts for: new arg pointer + NULL terminator
		if ( reserved_bytes >= ARG_MAX ) {
			result = E2BIG;
			goto fail;
		}

		size_t available = ARG_MAX - reserved_bytes;
		size_t single_arg_bytes;

		result = copyinstr(
			(const_userptr_t)user_arg,
			kargs + argsbytes, 
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

	/* Build stack layout: [argv pointers][argv strings][padding]
	 * argv_vector_base must be 8-byte aligned for MIPS ABI */
	argv_strings_base = stackptr - argsbytes;
	argv_strings_base_aligned = argv_strings_base & ~(vaddr_t)3; /* Align down to 4 bytes: clear the 2 lowest bits */
	argv_vector_base = argv_strings_base_aligned - (argc + 1) * sizeof(vaddr_t);
	argv_vector_base_aligned = argv_vector_base & ~(vaddr_t)7; /* Align down to 8 bytes: clear the 3 lowest bits */

	argsoffset = 0;
	for (int argnum = 0; argnum < argc; argnum++) {
		size_t arglen = strlen(kargs + argsoffset) + 1; /* The +1 accounts for the string terminator '\0' */

		argaddr = argv_strings_base + argsoffset;
		result = copyout(
			&argaddr,
			(userptr_t)(argv_vector_base + argnum * sizeof(vaddr_t)),
			sizeof(argaddr)
		);
		if (result) {
			goto fail;
		}

		result = copyoutstr(
			kargs + argsoffset,
			(userptr_t)(argv_strings_base + argsoffset), 
			arglen, 
			NULL
		);
		if (result) {
			goto fail;
		}

		argsoffset += arglen;
	}

	argaddr = 0; // NULL pointer
	result = copyout(
		&argaddr,
		(userptr_t)(argv_vector_base + argc * sizeof(vaddr_t)),
		sizeof(argaddr)
	);
	if (result) {
		goto fail;
	}

	if (oldas != NULL) {
		as_destroy(oldas);
	}
	kfree(kargs);
	kfree(kprogram);

	/* Pass control to new process - does not return on success */
	enter_new_process(argc, (userptr_t)argv_vector_base, NULL, argv_vector_base_aligned, entrypoint);

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
	if (kargs != NULL) {
		kfree(kargs);
	}
	if (kprogram != NULL) {
		kfree(kprogram);
	}
	return result;
}
