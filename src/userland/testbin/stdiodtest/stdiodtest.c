/*
 * stdiodtest.c
 *
 * Verifies that a newly started program has usable standard descriptors.
 */

#include <sys/types.h>
#include <unistd.h>

#define STDOUT_FAILURE 1
#define STDERR_FAILURE 2
#define STDIN_FAILURE 3
#define ECHO_FAILURE 4

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
	char echo_prefix[] = "\nstdiodtest: stdin returned: ";
	char success_message[] = "\nstdiodtest: PASS\n";

	if (write_failed(STDOUT_FILENO, stdout_message, sizeof(stdout_message) - 1)) { // -1 --> '\0'
		return STDOUT_FAILURE;
	}

	if (write_failed(STDERR_FILENO, stderr_message, sizeof(stderr_message) - 1)) {
		return STDERR_FAILURE;
	}

	if (write_failed(STDOUT_FILENO, input_prompt, sizeof(input_prompt) - 1)) {
		return STDOUT_FAILURE;
	}

	if (read_failed(STDIN_FILENO, &character, 1)) {
		return STDIN_FAILURE;
	}

	if (write_failed(STDOUT_FILENO, echo_prefix, sizeof(echo_prefix) - 1) ||
	    write_failed(STDOUT_FILENO, &character, 1) ||
	    write_failed(STDOUT_FILENO, success_message, sizeof(success_message) - 1)) {
		return ECHO_FAILURE;
	}

	return 0;
}