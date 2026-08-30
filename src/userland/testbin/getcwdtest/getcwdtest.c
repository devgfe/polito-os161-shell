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
	const char expected_initial[] = "emu0:";
	const char expected_directory[] = "emu0:/testbin";
	char buffer[64];
	int result;

	/*
	 * The kernel boot sets the initial cwd to "emu0:" via
	 * vfs_setbootfs("emu0") (see boot() in src/kern/main/main.c).
	 * New processes inherit it from the parent (proc_create_runprogram).
	 * __getcwd must write "emu0:" into the buffer and return 5.
	 */
	errno = 0;
	result = __getcwd(buffer, sizeof(buffer));
	if (result >= 0) {
		buffer[result] = '\0';
	}
	if (result == (int)(sizeof(expected_initial) - 1) &&
	    memcmp(buffer, expected_initial, sizeof(expected_initial) - 1) == 0) {
		printf("[INFO] initial cwd is \"%s\" (length %d)\n", buffer, result);
		pass("__getcwd returns bootfs root emu0:");
	}
	else {
		printf("[FAIL] __getcwd returns bootfs root emu0: (got %s, length %d, errno %d)\n",
		       result >= 0 ? buffer : "<unavailable>", result, errno);
		record_failure();
	}

	if (chdir("/testbin") < 0) {
		fail_errno("chdir for __getcwd test", 0, errno);
		return finish_test("getcwdtest");
	}

	/* After chdir("/testbin"), __getcwd must write "emu0:/testbin" into the buffer and return 13. */
	errno = 0;
	result = __getcwd(buffer, sizeof(buffer));
	if (result >= 0) {
		buffer[result] = '\0';
	}
	if (result == (int)(sizeof(expected_directory) - 1) &&
	    memcmp(buffer, expected_directory, sizeof(expected_directory) - 1) == 0) {
		printf("[INFO] after chdir(\"/testbin\"), cwd is \"%s\"\n", buffer);
		pass("__getcwd returns emu0:/testbin");
	}
	else {
		printf("[FAIL] __getcwd returns emu0:/testbin (got %s, length %d, errno %d)\n",
		       result >= 0 ? buffer : "<unavailable>", result, errno);
		record_failure();
	}

	errno = 0;
	result = __getcwd(NULL, sizeof(buffer));
	expect_errno("EFAULT: NULL buffer", result == -1, EFAULT, errno);

	errno = 0;
	result = __getcwd((char *)BAD_USER_POINTER, sizeof(buffer));
	expect_errno("EFAULT: invalid buffer", result == -1, EFAULT, errno);

	return finish_test("getcwdtest");
}