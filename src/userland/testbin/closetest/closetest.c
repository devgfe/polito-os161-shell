/*
 * closetest.c
 *
 * Tests successful close calls and deterministic errors from the manual page.
 */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include "../testreport.h"

#define TEST_FILE "/testbin/closetest.tmp"

int
main(void)
{
	int filehandle;
	int result;

	filehandle = open(TEST_FILE, O_RDONLY | O_CREAT);
	if (filehandle < 0) {
		fail_errno("open for close test", 0, errno);
		return finish_test("closetest");
	}

	if (close(filehandle) == 0) {
		pass("close returns zero for an open descriptor");
		errno = 0;
		result = close(filehandle);
		expect_errno("EBADF: descriptor already closed", result == -1, EBADF, errno);
	}
	else {
		fail("close returns zero for an open descriptor");
	}

	errno = 0;
	result = close(-1);
	expect_errno("EBADF: invalid descriptor", result == -1, EBADF, errno);

	return finish_test("closetest");
}