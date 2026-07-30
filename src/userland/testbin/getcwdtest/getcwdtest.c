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
	const char expected_directory[] = "/testbin";
	char buffer[64];
	ssize_t result;

	if (chdir(expected_directory) < 0) {
		fail_errno("chdir for __getcwd test", 0, errno);
		return finish_test("getcwdtest");
	}

	result = __getcwd(buffer, sizeof(buffer));
	if (result == (ssize_t)(sizeof(expected_directory) - 1) &&
	    memcmp(buffer, expected_directory, sizeof(expected_directory) - 1) == 0) {
		pass("__getcwd returns /testbin");
	}
	else {
		fail("__getcwd returns /testbin");
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