/*
 * getcwdtest.c
 *
 * Tests successful __getcwd calls and deterministic errors from the manual page.
 */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../testreport.h"

#define BAD_USER_POINTER ((void *)0x40000000)

int
main(void)
{
	/*
	 * The kernel boot sets the initial cwd to "emu0:" via
	 * vfs_setbootfs("emu0") (see boot() in src/kern/main/main.c).
	 * New processes inherit it from the parent (proc_create_runprogram).
	 * So at launch the cwd is "emu0:"; we print it for diagnosis, then
	 * change to /testbin for the deterministic check below.
	 */
	const char expected_directory[] = "emu0:/testbin";
	char buffer[64];
	int result;

	result = __getcwd(buffer, sizeof(buffer));
	if (result >= 0) {
		buffer[result] = '\0';
		printf("INFO: initial cwd is \"%s\" (length %d)\n", buffer, result);
	}
	else {
		printf("INFO: initial cwd lookup failed (errno %d)\n", errno);
	}

	if (chdir("/testbin") < 0) {
		fail_errno("chdir for __getcwd test", 0, errno);
		return finish_test("getcwdtest");
	}

	errno = 0;
	result = __getcwd(buffer, sizeof(buffer));
	if (result == (int)(sizeof(expected_directory) - 1) &&
	    memcmp(buffer, expected_directory, sizeof(expected_directory) - 1) == 0) {
		buffer[result] = '\0';
		printf("INFO: after chdir(\"/testbin\"), cwd is \"%s\"\n", buffer);
		pass("__getcwd returns emu0:/testbin");
	}
	else if (result == -1 && errno == ENOSYS) {
		/*
		 * emu0 is emufs (see root/sys161.conf); emufs_namefile()
		 * returns ENOSYS (errno 1) for any non-root vnode, so
		 * __getcwd can only succeed at the root.  Treat it as a
		 * skip, not a failure, on emufs-bootstrapped setups.
		 */
		printf("SKIP: __getcwd on /testbin (emufs supports only root getcwd, errno ENOSYS)\n");
	}
	else {
		printf("FAIL: __getcwd returns emu0:/testbin (got %s, length %d, errno %d)\n",
		       result >= 0 ? buffer : "<unavailable>", result, errno);
		record_failure();
	}

	errno = 0;
	result = __getcwd(NULL, sizeof(buffer));
	expect_errno("EFAULT: NULL buffer", result == -1, EFAULT, errno);

	errno = 0;
	result = __getcwd((char *)BAD_USER_POINTER, sizeof(buffer));
	expect_errno("EFAULT: invalid buffer", result == -1, EFAULT, errno);

	if (chdir("/") < 0) {
		fail_errno("chdir for __getcwd test", 0, errno);
	}

	return finish_test("getcwdtest");
}