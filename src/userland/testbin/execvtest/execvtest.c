/*
 * execvtest.c
 *
 * Tests successful execv calls and deterministic errors from the manual page.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "../testreport.h"

#define BAD_USER_POINTER ((void *)0x40000000)

/*
 * The child communicates WHY execv failed back to the parent through
 * its exit status: on failure it exits with the errno value, and the
 * parent compares it with the expected one.
 *
 * The two sentinel values below report test-harness problems instead
 * of an errno. They are chosen well above every errno value defined
 * in <kern/errno.h> (which currently go up to 64), so they can never
 * be mistaken for a real errno produced by execv. If new errno values
 * are ever added to the kernel, they must stay below these sentinels.
 */
#define CHILD_MISSING_ERRNO 100		/* execv failed but left errno == 0 */
#define CHILD_UNEXPECTED_RETURN 101	/* execv returned something != -1 */

#define ARGTEST_PATH "/testbin/argtest"

static
void
expect_exec(const char *description, const char *program, char **args,
            int expected_exit_status)
{
	pid_t pid;
	pid_t waited_pid;
	int status;
	int result;
	int child_status;

	pid = fork();
	if (pid < 0) {
		fail_errno("fork for execv test", 0, errno);
		return;
	}

	if (pid == 0) {
		/*
		 * Child: try to replace this process image.
		 * On success execv does not return at all. On failure it
		 * returns -1 and sets errno: report the errno to the parent
		 * as our exit status. The two sentinel exit codes report
		 * contract violations instead (see their definitions above).
		 */
		errno = 0;
		result = execv(program, args);
		if (result != -1) {
			_exit(CHILD_UNEXPECTED_RETURN);
		}
		else if (errno == 0) {
			_exit(CHILD_MISSING_ERRNO);
		}
		else {
			_exit(errno);
		}
	}

	/* Parent */
	waited_pid = waitpid(pid, &status, 0);
	if (waited_pid != pid) {
		fail_errno("waitpid for execv test", 0, errno);
		return;
	}

	if (WIFEXITED(status)) {
		child_status = WEXITSTATUS(status);
		if (child_status == expected_exit_status) {
			pass(description);
		} 
		else if (child_status == CHILD_MISSING_ERRNO) {
			fail("execv failed without setting errno");
		} 
		else if (child_status == CHILD_UNEXPECTED_RETURN) {
			fail("execv returned a value other than -1");
		} 
		else {
			fail_errno("execv returned an unexpected exit status", expected_exit_status, child_status);
		}
	}
	else if (WIFSIGNALED(status)) {
		fail("execv child received a signal");
	}
	else {
		fail("execv child returned an unexpected status");
	}
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

	expect_exec("successful execv ran argtest", ARGTEST_PATH, valid_args, 0);
	expect_exec("ENODEV: unknown device prefix", "no-such-device:/bin/true", valid_args, ENODEV);
	expect_exec("ENOTDIR: regular file in path", "/bin/true/not-a-program", valid_args, ENOTDIR);
	expect_exec("ENOENT: missing program", "/testbin/execvtest-no-such-program", valid_args, ENOENT);
	expect_exec("EISDIR: program is a directory", "/testbin", valid_args, EISDIR);
	expect_exec("ENOEXEC: non-ELF program", "/sys161.conf", valid_args, ENOEXEC);
	/* ENOMEM case depends on configured RAM (current sys161.conf uses 2M). */
	expect_exec("ENOMEM: insufficient RAM for huge program", "/testbin/huge", valid_args, ENOMEM);
	expect_exec("E2BIG: argument exceeds ARG_MAX", ARGTEST_PATH, oversized_args, E2BIG);
	expect_exec("EFAULT: NULL program pointer", NULL, valid_args, EFAULT);
	expect_exec("EFAULT: invalid program pointer", (const char *)BAD_USER_POINTER, valid_args, EFAULT);
	expect_exec("EFAULT: NULL argv pointer", ARGTEST_PATH, NULL, EFAULT);
	expect_exec("EFAULT: invalid argv pointer", ARGTEST_PATH, (char **)BAD_USER_POINTER, EFAULT);
	expect_exec("EFAULT: invalid argument string", ARGTEST_PATH, bad_string_args, EFAULT);

	return finish_test("execvtest");
}