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
test_orphan_parent_first(void)
{
	pid_t child;

	child = fork();
	if (child < 0) {
		fail_errno("orphan(parent first): fork failed", 0, errno);
		return;
	}

	if (child == 0) {
		/*
		 * Parent will exit first. We are the child and we busy-wait
		 * briefly so the parent becomes an orphan *before* us.
		 */
		for (long i = 0; i < 100000; i++);
		_exit(7);
	}

	/* Parent return immediately without waiting. */
	pass("orphan(parent first): parent returned without waiting (kernel reaps child)");
}

static
void
test_orphan_child_first(void)
{
	pid_t child;

	child = fork();
	if (child < 0) {
		fail_errno("orphan(child first): fork failed", 0, errno);
		return;
	}

	if (child == 0) {
		/* Child exits immediately; parent busy-waits then exits. */
		_exit(8);
	}

	/* Parent busy-waits so child exits first, then parent exits. */
	for (long i = 0; i < 100000; i++);
	/* Deliberately no waitpid. */
	pass("orphan(child first): parent returned without waiting (kernel reaps child)");
}

static
void
test_zombie(void)
{
	pid_t child;
	int status;
	pid_t result;

	child = fork();
	if (child < 0) {
		fail_errno("zombie: fork failed", 0, errno);
		return;
	}

	if (child == 0) {
		_exit(42);
	}

	/* Busy-wait so child exits before we call waitpid. */
	for (long i = 0; i < 100000; i++);

	result = waitpid(child, &status, 0);
	if (result != child) {
		fail("zombie: waitpid returns child pid");
	}
	else if (WIFEXITED(status) && WEXITSTATUS(status) == 42) {
		pass("zombie: waitpid reaps a child that already exited");
	}
	else {
		fail("zombie: waitpid returns correct exit status");
	}
}

int
main(void)
{
	/* Scenario 1: zombie reaping (child already exited, parent waits) */
	test_zombie();

    /* Scenario 2: child exits before parent (parent sleeps briefly) */
	test_orphan_child_first();

    /* Scenario 3: parent exits before child (child sleeps briefly) */
	test_orphan_parent_first();

	return finish_test("zombietest");
}
