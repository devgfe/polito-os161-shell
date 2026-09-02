/*
 * zombietest.c
 *
 * Tests orphan and zombie process handling.
 *
 * ORPHAN: a child whose parent exited without calling waitpid; the
 *         kernel must reap it on its own.
 * ZOMBIE: a child that exited but has not been reaped by its parent;
 *         waitpid must return its exit status and destroy it.
 */

#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "../testreport.h"

static
void
spin_delay(volatile unsigned long count)
{
	volatile unsigned long i;

	for (i = 0; i < count; i++) {
		/* Intentional busy wait for test ordering. */
	}
}

static
int
test_orphan_parent_first(void)
{
	pid_t child;

	child = fork();
	if (child < 0) {
		fail_errno("orphan(parent first): fork failed", 0, errno);
		return 1;
	}

	if (child == 0) {
		/*
		 * Parent will exit first. We are the child and we busy-wait
		 * briefly so the parent becomes an orphan *before* us.
		 */
		spin_delay(2000000);
		_exit(7);
	}

	/* Parent returns immediately without waiting. */
	return 0;
}

static
int
test_orphan_child_first(void)
{
	pid_t child;

	child = fork();
	if (child < 0) {
		fail_errno("orphan(child first): fork failed", 0, errno);
		return 1;
	}

	if (child == 0) {
		/* Child exits immediately; parent busy-waits then exits. */
		_exit(8);
	}

	/* Parent busy-waits so child exits first, then parent exits. */
	spin_delay(2000000);
	/* Deliberately no waitpid. */
	return 0;
}

static
int
test_zombie(void)
{
	pid_t child;
	int status;
	pid_t result;

	child = fork();
	if (child < 0) {
		fail_errno("zombie: fork failed", 0, errno);
		return 1;
	}

	if (child == 0) {
		_exit(42);
	}

	/* Busy-wait so child exits before we call waitpid. */
	spin_delay(2000000);

	result = waitpid(child, &status, 0);
	if (result < 0) {
		fail_errno("zombie: waitpid failed", 0, errno);
		return 1;
	}
	if (result != child) {
		fail("zombie: waitpid returns child pid");
		return 1;
	}

	if (WIFEXITED(status) && WEXITSTATUS(status) == 42) {
		return 0;
	}
	else {
		fail("zombie: waitpid returns correct exit status");
		return 1;
	}
}

static
void
run_test_in_wrapper(const char *name, int (*testfn)(void))
{
	pid_t wrapper;
	pid_t result;
	int status;

	wrapper = fork();
	if (wrapper < 0) {
		fail_errno("wrapper: fork failed", 0, errno);
		return;
	}

	if (wrapper == 0) {
		/* Wrapper */
		_exit(testfn() == 0 ? 0 : 1);
	}

	/* Main */
	result = waitpid(wrapper, &status, 0);
	if (result < 0) {
		fail_errno("wrapper: waitpid failed", 0, errno);
		return;
	}
	if (result != wrapper) {
		fail("wrapper: waitpid returns wrapper pid");
		return;
	}

	if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
		fail(name);
	}
}

int
main(void)
{
	printf("[INFO] For detailed fork/waitpid/exit traces, enable kernel option procdebug.\n");

	run_test_in_wrapper("zombie", test_zombie);
	run_test_in_wrapper("orphan(child first)", test_orphan_child_first);
	run_test_in_wrapper("orphan(parent first)", test_orphan_parent_first);

	return finish_test("zombietest");
}
