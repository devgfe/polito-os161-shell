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

/* Replace the current process image. */
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

	kprogram = kmalloc(PATH_MAX);
	if (kprogram == NULL) {
		result = ENOMEM;
		goto fail;
	}

	result = copyinstr(user_program, kprogram, PATH_MAX, NULL);
	if (result) {
		goto fail;
	}

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

		size_t reserved_bytes = argsbytes + (argc + 2) * sizeof(vaddr_t); // +2 --> new arg + NULL
		if ( reserved_bytes >= ARG_MAX ) {
			result = E2BIG;
			goto fail;
		}

		size_t available = ARG_MAX - reserved_bytes;
		size_t argbytes;

		result = copyinstr(
			(const_userptr_t)user_arg,
			kargs + argsbytes, 
			available, 
			&argbytes
		);
		if (result) {
			if (result == ENAMETOOLONG) result = E2BIG;
			goto fail;
		}

		argsbytes += argbytes;
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

	argv_strings_base = stackptr - argsbytes;
	argv_strings_base_aligned = argv_strings_base & ~(vaddr_t)3; // 3 (dec) = 11 (bin) --> 2^2 * 1 byte --> 4 byte
	argv_vector_base = argv_strings_base_aligned - (argc + 1) * sizeof(vaddr_t);
	argv_vector_base_aligned = argv_vector_base & ~(vaddr_t)7; // 7 (dec) = 111 (bin) --> 2^3 * 1 byte --> 8 byte

	argsoffset = 0;
	for (int argnum = 0; argnum < argc; argnum++) {
		size_t arglen = strlen(kargs + argsoffset) + 1; // +1 --> '\0'

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

	argaddr = 0; // NULL
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
	enter_new_process(argc, (userptr_t)argv_vector_base, NULL, argv_vector_base_aligned, entrypoint);

	panic("enter_new_process returned\n");
	return EINVAL;

fail:
	if (v != NULL) {
		vfs_close(v);
	}
	if (switched) {
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
