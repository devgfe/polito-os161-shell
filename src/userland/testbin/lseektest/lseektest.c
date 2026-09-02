/*
 * lseektest.c
 *
 * Tests successful lseek calls and deterministic errors from the manual page.
 *
 * The test file contains "abcdef" (6 bytes), so offsets and characters
 * below refer to that content:
 *
 *   offset:  0    1    2    3    4    5	6
 *   content: 'a'  'b'  'c'  'd'  'e'  'f'	EOF
 */

#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "../testreport.h"

#define TEST_FILE "/testbin/lseektest.tmp"

static
int
prepare_test_file(const char *contents, size_t length)
{
	int filehandle;

	filehandle = open(TEST_FILE, O_WRONLY | O_CREAT | O_TRUNC);
	if (filehandle < 0) {
		fail_errno("open for lseek test", 0, errno);
		return -1;
	}
	if (write(filehandle, contents, length) != (ssize_t)length) {
		fail_errno("write for lseek test", 0, errno);
		close(filehandle);
		return -1;
	}
	if (close(filehandle) < 0) {
		fail_errno("close for lseek test", 0, errno);
		return -1;
	}
	return 0;
}
int
main(void)
{
	const char contents[] = "abcdef";
	int devicehandle;
	int filehandle;
	off_t result;
	char character;

	if (prepare_test_file(contents, sizeof(contents) - 1) != 0) {
		return finish_test("lseektest");
	}

	filehandle = open(TEST_FILE, O_RDWR);
	if (filehandle < 0) {
		fail_errno("open for lseek test", 0, errno);
		return finish_test("lseektest");
	}

	result = lseek(filehandle, 2, SEEK_SET);
	if (result == 2 && read(filehandle, &character, 1) == 1 && character == 'c') {
		pass("SEEK_SET returns the new offset");
	}
	else {
		fail("SEEK_SET returns the new offset");
	}
	result = lseek(filehandle, 1, SEEK_CUR);
	if (result == 4 && read(filehandle, &character, 1) == 1 && character == 'e') {
		pass("SEEK_CUR advances from the current offset");
	}
	else {
		fail("SEEK_CUR advances from the current offset");
	}
	result = lseek(filehandle, 0, SEEK_END);
	if (result == 6 && read(filehandle, &character, 1) == 0) {
		pass("SEEK_END returns the end-of-file offset");
	}
	else {
		fail("SEEK_END returns the end-of-file offset");
	}

	errno = 0;
	result = lseek(-1, 0, SEEK_SET);
	expect_errno("EBADF: invalid descriptor", result == -1, EBADF, errno);

	errno = 0;
	result = lseek(filehandle, -1, SEEK_SET);
	expect_errno("EINVAL: negative resulting offset", result == -1, EINVAL, errno);

	errno = 0;
	result = lseek(filehandle, 0, 12345);
	expect_errno("EINVAL: invalid whence", result == -1, EINVAL, errno);
	close(filehandle);

	/*
	 * ESPIPE: fd refers to an object which does not support seeking.
	 * The null: device is a pure character device with no notion of a
	 * file position, so every seek on it must fail with ESPIPE.
	 */
	devicehandle = open("null:", O_RDONLY);
	if (devicehandle < 0) {
		fail_errno("open for lseek test", 0, errno);
	}
	else {
		errno = 0;
		result = lseek(devicehandle, 0, SEEK_SET);
		expect_errno("ESPIPE: non-seekable device", result == -1, ESPIPE, errno);
		close(devicehandle);
	}

	return finish_test("lseektest");
}