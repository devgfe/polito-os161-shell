/*
 * stdiodtest.c
 *
 * Verifies that a newly started program has usable standard descriptors.
 */

#include <sys/types.h>
#include <errno.h>
#include <unistd.h>

#include "../testreport.h"

#define EXPECTED_CHAR 'x'

static
int
write_failed(int filehandle, const char *buffer, size_t length)
{
	ssize_t result;

	result = write(filehandle, buffer, length);
	return result != (ssize_t)length;
}

static
int
read_failed(int filehandle, char *buffer, size_t length)
{
	ssize_t result;

	result = read(filehandle, buffer, length);
	return result != (ssize_t)length;
}

int
main(void)
{
	char character;

	char stdout_message[] = "stdiodtest: write to stdout (fd 1) succeeded\n";
	char stderr_message[] = "stdiodtest: write to stderr (fd 2) succeeded\n";
	char input_prompt[] = "stdiodtest: type one character for stdin (fd 0): ";
	char echo_prefix[] = "stdiodtest: stdin returned: ";
	char success_message[] = "\nstdiodtest: PASS\n";

	character = '\0';

	if (write_failed(STDOUT_FILENO, stdout_message, sizeof(stdout_message) - 1)) {
		fail_errno("write to stdout (fd 1)", 0, errno);
	}
	else {
		pass("write to stdout (fd 1)");
	}

	if (write_failed(STDERR_FILENO, stderr_message, sizeof(stderr_message) - 1)) {
		fail_errno("write to stderr (fd 2)", 0, errno);
	}
	else {
		pass("write to stderr (fd 2)");
	}

	if (write_failed(STDOUT_FILENO, input_prompt, sizeof(input_prompt) - 1)) {
		fail_errno("write stdin prompt to stdout", 0, errno);
	}
	else {
		pass("write stdin prompt to stdout");
	}

	if (read_failed(STDIN_FILENO, &character, 1)) {
		fail_errno("read from stdin (fd 0)", 0, errno);
	}
	else if (character != EXPECTED_CHAR) {
		fail("read from stdin (fd 0): wrong character");
	}
	else {
		pass("read from stdin (fd 0)");
	}

	if (write_failed(STDOUT_FILENO, echo_prefix, sizeof(echo_prefix) - 1) ||
	    write_failed(STDOUT_FILENO, &character, 1) ||
	    write_failed(STDOUT_FILENO, success_message, sizeof(success_message) - 1)) {
		fail_errno("write echo to stdout", 0, errno);
	}
	else {
		pass("write echo to stdout");
	}

	return finish_test("stdiodtest");
}