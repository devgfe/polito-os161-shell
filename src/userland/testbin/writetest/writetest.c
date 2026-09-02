/*
 * writetest.c
 *
 * Tests successful write calls and deterministic errors from the manual page.
 */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "../testreport.h"

#define BAD_USER_POINTER ((void *)0x40000000)
#define TEST_FILE "/testbin/writetest.tmp"

static
int
prepare_test_file(const char *contents, size_t length)
{
	int filehandle;

	filehandle = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC);
	if (filehandle < 0) {
		fail_errno("open for write test", 0, errno);
		return -1;
	}

	if (write(filehandle, contents, length) != (ssize_t)length) {
		fail_errno("write for write test", 0, errno);
		close(filehandle);
		return -1;
	}

	if (close(filehandle) < 0) {
		fail_errno("close for write test", 0, errno);
		return -1;
	}

	return 0;
}

int
main(void)
{
	const char contents[] = "write syscall data";
	char buffer[sizeof(contents)];
	int filehandle;
	ssize_t result;

	if (prepare_test_file(contents, sizeof(contents) - 1) != 0) {
		return finish_test("writetest");
	}
	pass("write returns the requested byte count");

	filehandle = open(TEST_FILE, O_RDONLY);
	if (filehandle < 0) {
		fail_errno("open for write test", 0, errno);
	}
	else {
		result = read(filehandle, buffer, sizeof(contents) - 1);
		if (result == (ssize_t)(sizeof(contents) - 1) &&
		    memcmp(buffer, contents, sizeof(contents) - 1) == 0) {
			pass("write stores the requested data");
		}
		else if (result < 0) {
			fail_errno("read for write test", 0, errno);
		}
		else {
			fail("write stores the requested data");
		}

		if (close(filehandle) < 0) {
			fail_errno("close for write test", 0, errno);
		}
	}

	errno = 0;
	result = write(-1, contents, 1);
	expect_errno("EBADF: invalid descriptor", result == -1, EBADF, errno);

	filehandle = open(TEST_FILE, O_RDONLY);
	if (filehandle < 0) {
		fail_errno("open for write test", 0, errno);
	}
	else {
		errno = 0;
		result = write(filehandle, contents, 1);
		expect_errno("EBADF: read-only descriptor", result == -1, EBADF, errno);
		close(filehandle);
	}

	filehandle = open(TEST_FILE, O_WRONLY);
	if (filehandle < 0) {
		fail_errno("open for write test", 0, errno);
	}
	else {
		errno = 0;
		result = write(filehandle, BAD_USER_POINTER, 1);
		expect_errno("EFAULT: invalid buffer", result == -1, EFAULT, errno);
		close(filehandle);
	}

	return finish_test("writetest");
}