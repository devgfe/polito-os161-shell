/*
 * getpidtest.c
 *
 * Tests successful getpid calls and deterministic errors from the manual page.
 */

#include <stdio.h>
#include <unistd.h>

#include "../testreport.h"

int
main(void)
{
	pid_t first;
	pid_t second;

	first = getpid();
	second = getpid();
	if (first > 0) {
		pass("getpid returns a positive process id");
	}
	else {
		fail("getpid returns a positive process id");
	}
	if (first == second) {
		pass("getpid is stable in the same process");
	}
	else {
		fail("getpid is stable in the same process");
	}

	return finish_test("getpidtest");
}