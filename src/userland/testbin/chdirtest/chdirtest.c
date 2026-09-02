/*
 * chdirtest.c
 *
 * Tests successful chdir calls and deterministic errors from the manual page.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "../testreport.h"

#define BAD_USER_POINTER ((void *)0x40000000)

int
main(void)
{
	int filehandle;
	int result;

	if (chdir("/testbin") == 0) {
		filehandle = open("argtest", O_RDONLY);
		if (filehandle >= 0) {
			pass("chdir changes to an existing directory");
			if (close(filehandle) < 0) {
				fail_errno("close for chdir test", 0, errno);
			}
		}
		else {
			fail_errno("open for chdir test", 0, errno);
		}
	}
	else {
		fail_errno("chdir changes to an existing directory", 0, errno);
	}

	if (chdir("/") == 0) {
		pass("chdir changes back to root");
	}
	else {
		fail_errno("chdir changes back to root", 0, errno);
	}

	errno = 0;
	result = chdir("no-such-device:/directory");
	expect_errno("ENODEV: unknown device prefix", result == -1, ENODEV, errno);

	errno = 0;
	result = chdir("/bin/true");
	expect_errno("ENOTDIR: regular file", result == -1, ENOTDIR, errno);

	errno = 0;
	result = chdir("/chdirtest-does-not-exist");
	expect_errno("ENOENT: missing path", result == -1, ENOENT, errno);

	errno = 0;
	result = chdir(NULL);
	expect_errno("EFAULT: NULL pathname", result == -1, EFAULT, errno);

	errno = 0;
	result = chdir((const char *)BAD_USER_POINTER);
	expect_errno("EFAULT: invalid pathname", result == -1, EFAULT, errno);

	return finish_test("chdirtest");
}