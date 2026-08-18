/*
 * dup2test.c
 *
 * Tests successful dup2 calls and deterministic errors from the manual page.
 */

#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../testreport.h"

#define TEST_FILE "/testbin/dup2test.tmp"

static
int
prepare_test_file(const char *contents, size_t length)
{
	int filehandle;

	filehandle = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC);
	if (filehandle < 0) {
		fail_errno("open for dup2 test", 0, errno);
		return -1;
	}
	if (write(filehandle, contents, length) != (ssize_t)length) {
		fail_errno("write for dup2 test", 0, errno);
		close(filehandle);
		return -1;
	}
	if (close(filehandle) < 0) {
		fail_errno("close for dup2 test", 0, errno);
		return -1;
	}
	return 0;
}

int
main(void)
{
	const char contents[] = "ab";
	char first;
	char second;
	int filehandle;
	int targethandle;
	int result;
	int duplicated;

	if (prepare_test_file(contents, sizeof(contents) - 1) != 0) {
		return finish_test("dup2test");
	}

	filehandle = open(TEST_FILE, O_RDONLY);
	if (filehandle < 0) {
		fail_errno("open for dup2 test", 0, errno);
		return finish_test("dup2test");
	}

	targethandle = filehandle == 3 ? 4 : 3;
	result = dup2(filehandle, targethandle);
	duplicated = result == targethandle;
	if (duplicated) {
		pass("dup2 returns its target descriptor");
	}
	else {
		fail("dup2 returns its target descriptor");
	}

	if (duplicated &&
	    read(filehandle, &first, 1) == 1 && first == 'a' &&
	    read(targethandle, &second, 1) == 1 && second == 'b') {
		pass("duplicated descriptors share the seek offset");
	}
	else {
		fail("duplicated descriptors share the seek offset");
	}

	if (dup2(filehandle, filehandle) == filehandle) {
		pass("dup2 onto itself preserves the descriptor");
	}
	else {
		fail("dup2 onto itself preserves the descriptor");
	}

	if (duplicated) {
		close(targethandle);
	}

	errno = 0;
	result = dup2(-1, filehandle);
	expect_errno("EBADF: invalid source descriptor", result == -1, EBADF,
	             errno);

	errno = 0;
	result = dup2(filehandle, -1);
	expect_errno("EBADF: invalid target descriptor", result == -1, EBADF,
	             errno);

	/*
	 * EBADF: target descriptor that can never be valid.
	 * OPEN_MAX is the maximum number of open files per process, so no
	 * descriptor >= OPEN_MAX can exist.
	 */
	errno = 0;
	result = dup2(filehandle, OPEN_MAX + 1);
	expect_errno("EBADF: impossible target descriptor", result == -1, EBADF,
	             errno);
	close(filehandle);

	return finish_test("dup2test");
}