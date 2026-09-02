/*
 * Shared helper for building the initial user-level
 * stack used by both runprogram() and sys_execv().
 */

#include <initstack.h>
#include <copyinout.h>
#include <lib.h>

/*
 * initstack - build on the user stack, starting from
 * stackptr (from as_define_stack), the argv strings followed by the
 * argv pointer vector terminated by NULL.
 *
 * Puts in *ret_argv the user-space address of the argv vector,
 * aligned to 8 bytes; it doubles as the new stack pointer to pass
 * to enter_new_process().
 */
int
initstack(char **kargv, int argc, vaddr_t stackptr, userptr_t *ret_argv)
{
	vaddr_t argv_strings_base;
	vaddr_t argv_vector_base;
	vaddr_t argv_strings_base_aligned;
	vaddr_t argv_vector_base_aligned;
	vaddr_t argaddr;
	size_t argsbytes;
	size_t argsoffset;
	int argnum;
	int result;

	/* Total bytes occupied by the strings */
	argsbytes = 0;
	for (argnum = 0; argnum < argc; argnum++) {
		argsbytes += strlen(kargv[argnum]) + 1; // +1 --> '\0'
	}

	argv_strings_base = stackptr - argsbytes;
	argv_strings_base_aligned = argv_strings_base & ~(vaddr_t)3; /* Align down to 4 bytes: clear the 2 lowest bits */
	argv_vector_base = argv_strings_base_aligned - (argc + 1) * sizeof(vaddr_t); // +1 --> NULL
	argv_vector_base_aligned = argv_vector_base & ~(vaddr_t)7; /* Align down to 8 bytes: clear the 3 lowest bits */

	argsoffset = 0;
	for (argnum = 0; argnum < argc; argnum++) {
		size_t arglen = strlen(kargv[argnum]) + 1; /* The +1 accounts for the string terminator '\0' */

		/* User-space address of this argument's string */
		argaddr = argv_strings_base_aligned + argsoffset;
		result = copyout(
			&argaddr,
			(userptr_t)(argv_vector_base_aligned + argnum * sizeof(vaddr_t)),
			sizeof(argaddr)
		);
		if (result) {
			return result;
		}

		result = copyoutstr(
			kargv[argnum],
			(userptr_t)argaddr,
			arglen,
			NULL
		);
		if (result) {
			return result;
		}

		argsoffset += arglen;
	}

	/* argv[argc] must be NULL, as required by the execv specification */
	argaddr = 0;
	result = copyout(
		&argaddr,
		(userptr_t)(argv_vector_base_aligned + argc * sizeof(vaddr_t)),
		sizeof(argaddr)
	);
	if (result) {
		return result;
	}

	*ret_argv = (userptr_t)argv_vector_base_aligned;
	return 0;
}