/*
 * readtest.c
 *
 * Tests successful read calls and deterministic errors from the manual page.
 */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "../testreport.h"

#define BAD_USER_POINTER ((void *)0x40000000)
#define TEST_FILE "/testbin/readtest.tmp"

static
int
prepare_test_file(const char *contents, size_t length)
{
	int filehandle;

	filehandle = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC);
	if (filehandle < 0) {
		fail_errno("open for read test", 0, errno);
		return -1;
	}

	if (write(filehandle, contents, length) != (ssize_t)length) {
		fail_errno("write for read test", 0, errno);
		close(filehandle);
		return -1;
	}

	if (close(filehandle) < 0) {
		fail_errno("close for read test", 0, errno);
		return -1;
	}

	return 0;
}
int
main(void)
{
	const char contents[] = "read syscall data";
	char buffer[sizeof(contents)];
	int filehandle;
	ssize_t result;

	if (prepare_test_file(contents, sizeof(contents) - 1) != 0) {
		return finish_test("readtest");
	}

	filehandle = open(TEST_FILE, O_RDONLY);
	if (filehandle < 0) {
		fail_errno("open for read test", 0, errno);
		return finish_test("readtest");
	}

	result = read(filehandle, buffer, sizeof(contents) - 1);
	if (result == (ssize_t)(sizeof(contents) - 1) &&
		memcmp(buffer, contents, sizeof(contents) - 1) == 0) {
		pass("read returns the requested byte count and stored data");
	}
	else {
		fail("read returns the requested byte count and stored data");
	}

	if (read(filehandle, buffer, 1) == 0) {
		pass("read returns zero at end of file");
	}
	else {
		fail("read returns zero at end of file");
	}
	close(filehandle);

	errno = 0;
	result = read(-1, buffer, 1);
	expect_errno("EBADF: invalid descriptor", result == -1, EBADF, errno);

	filehandle = open(TEST_FILE, O_WRONLY);
	if (filehandle < 0) {
		fail_errno("open for read test", 0, errno);
	}
	else {
		errno = 0;
		result = read(filehandle, buffer, 1);
		expect_errno("EBADF: write-only descriptor", result == -1, EBADF, errno);
		close(filehandle);
	}

	filehandle = open(TEST_FILE, O_RDONLY);
	if (filehandle < 0) {
		fail_errno("open for read test", 0, errno);
	}
	else {
		errno = 0;
		result = read(filehandle, BAD_USER_POINTER, 1);
		expect_errno("EFAULT: invalid buffer", result == -1, EFAULT, errno);
		close(filehandle);
	}

	return finish_test("readtest");
}