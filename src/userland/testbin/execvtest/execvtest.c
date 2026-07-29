/*
 * execvtest.c
 *
 * Tests the error results required by the OS/161 execv manual page.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BAD_USER_POINTER ((void *)0x40000000)
#define CHILD_MISSING_ERRNO 100
#define CHILD_UNEXPECTED_RETURN 101
#define ARGTEST_PATH "/testbin/argtest"

struct exec_case {
	const char *description;
	const char *program;
	char **args;
	int expected_exit_status;
};

static
int
expect_exec(const struct exec_case *test_case)
{
	pid_t pid;
	pid_t waited_pid;
	int status;
	int result;
	int child_status;

	pid = fork();
	if (pid < 0) {
		printf("FAIL: %s (fork failed, errno %d)\n",
		       test_case->description, errno);
		return 1;
	}

	if (pid == 0) { 
		/* Child */
		errno = 0;
		result = execv(test_case->program, test_case->args);
		if (result != -1) {
			_exit(CHILD_UNEXPECTED_RETURN);
		}
		if (errno == 0) {
			_exit(CHILD_MISSING_ERRNO);
		}
		_exit(errno);
	}

	/* Parent */
	waited_pid = waitpid(pid, &status, 0);
	if (waited_pid != pid) {
		printf("FAIL: %s (waitpid failed, errno %d)\n",
		    test_case->description, errno);
		return 1;
	}

	if (WIFEXITED(status)) {
		child_status = WEXITSTATUS(status);
		if (child_status == test_case->expected_exit_status) {
			printf("PASS: %s\n", test_case->description);
			return 0;
		} 
		else if (child_status == CHILD_MISSING_ERRNO) {
			printf("FAIL: %s (execv failed without setting errno)\n",
		       	test_case->description);
		} 
		else if (child_status == CHILD_UNEXPECTED_RETURN) {
			printf("FAIL: %s (execv returned a value other than -1)\n",
		       	test_case->description);
		} 
		else {
			printf("FAIL: %s (expected status %d, got %d)\n",
	       		test_case->description, test_case->expected_exit_status,
	       		child_status);
		}
	}
	else if (WIFSIGNALED(status)) {
		printf("FAIL: %s (child received signal %d)\n", test_case->description,
		       WTERMSIG(status));
	}
	else {
		printf("FAIL: %s (unexpected child status %d)\n", test_case->description,
		       status);
	}

	return 1;
}

int
main(void)
{
	char *valid_args[] = {
		(char *)"argtest",
		(char *)"first argument",
		(char *)"second argument",
		NULL
	};
	char *bad_string_args[] = {
		(char *)"argtest",
		(char *)BAD_USER_POINTER,
		NULL
	};
	char *oversized_args[3];

	char oversized_argument[ARG_MAX + 2];
	memset(oversized_argument, 'x', sizeof(oversized_argument) - 1);
	oversized_argument[sizeof(oversized_argument) - 1] = '\0';

	oversized_args[0] = (char *)"argtest";
	oversized_args[1] = oversized_argument;
	oversized_args[2] = NULL;
	
	struct exec_case test_cases[] = {
		{ "successful execv ran argtest", ARGTEST_PATH, valid_args, 0 },
		{ "ENODEV: unknown device prefix", "no-such-device:/bin/true", valid_args, ENODEV },
		{ "ENOTDIR: regular file in path", "/bin/true/not-a-program", valid_args, ENOTDIR },
		{ "ENOENT: missing program", "/testbin/execvtest-no-such-program", valid_args, ENOENT },
		{ "EISDIR: program is a directory", "/testbin", valid_args, EISDIR },
		{ "ENOEXEC: non-ELF program", "/sys161.conf", valid_args, ENOEXEC },
		{ "E2BIG: argument exceeds ARG_MAX", ARGTEST_PATH, oversized_args, E2BIG },
		{ "EFAULT: NULL program pointer", NULL, valid_args, EFAULT },
		{ "EFAULT: invalid program pointer", (const char *)BAD_USER_POINTER, valid_args, EFAULT },
		{ "EFAULT: NULL argv pointer", ARGTEST_PATH, NULL, EFAULT },
		{ "EFAULT: invalid argv pointer", ARGTEST_PATH, (char **)BAD_USER_POINTER, EFAULT },
		{ "EFAULT: invalid argument string", ARGTEST_PATH, bad_string_args, EFAULT },
		{ "ENOMEM: insufficient RAM for huge program", "/testbin/huge", valid_args, ENOMEM }
	};

	for (int i = 0; i < sizeof(test_cases) / sizeof(struct exec_case); i++) {
		if (expect_exec(&test_cases[i]))
			return 1;
	}

	return 0;
}