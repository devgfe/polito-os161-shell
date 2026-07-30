/*
 * waitpidtest.c
 *
 * Tests successful waitpid calls and deterministic errors from the manual page.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "../testreport.h"

#define BAD_USER_POINTER ((void *)0x40000000)
#define EXIT_STATUS 73

static
pid_t
spawn_exit(int exit_status)
{
	pid_t child;

	child = fork();
	if (child < 0) {
		fail_errno("fork for waitpid test", 0, errno);
	}
	else if (child == 0) {
		_exit(exit_status);
	}
	return child;
}

static
void
reap_if_possible(pid_t child)
{
	int status;

	if (waitpid(child, &status, 0) != child) {
		fail("waitpid does not return the requested child pid");
	}
}

int
main(void)
{
	pid_t child;
	pid_t result;
	int status;

	child = spawn_exit(EXIT_STATUS);
	if (child > 0) {
		result = waitpid(child, &status, 0);
		if (result != child) {
			fail("waitpid returns the requested child pid");
			reap_if_possible(child);
		}
		else {
			pass("waitpid returns the requested child pid");

			if (WIFEXITED(status) && WEXITSTATUS(status) == EXIT_STATUS) {
				pass("_exit status is reported by waitpid");
			}
			else {
				fail("_exit status is reported by waitpid");
			}

			errno = 0;
			result = waitpid(child, &status, 0);
			expect_errno("ESRCH: already reaped child", result == -1, ESRCH, errno);
		}
	}

	child = spawn_exit(0);
	if (child > 0) {
		if (waitpid(child, NULL, 0) == child) {
			pass("waitpid accepts a NULL status pointer");
		}
		else {
			fail("waitpid accepts a NULL status pointer");
			reap_if_possible(child);
		}
	}

	child = spawn_exit(0);
	if (child > 0) {
		errno = 0;
		result = waitpid(child, (int *)BAD_USER_POINTER, 0);
		expect_errno("EFAULT: invalid status pointer", result == -1, EFAULT, errno);
		if (result != child) {
			reap_if_possible(child);
		}
	}

	child = spawn_exit(0);
	if (child > 0) {
		errno = 0;
		result = waitpid(child, &status, 1);
		expect_errno("EINVAL: unsupported options", result == -1, EINVAL, errno);
		if (result != child) {
			reap_if_possible(child);
		}
	}

	errno = 0;
	result = waitpid(getpid(), &status, 0);
	expect_errno("ECHILD: current process is not a child", result == -1, ECHILD, errno);

	return finish_test("waitpidtest");
}