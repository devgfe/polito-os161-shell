/* Shared reporting helpers for standalone userland tests. */

#ifndef TESTBIN_TESTREPORT_H
#define TESTBIN_TESTREPORT_H

#include <stdio.h>

static int failures;

static
void
record_failure(void)
{
	failures++;
}

static
void
pass(const char *description)
{
	printf("PASS: %s\n", description);
}

static
void
fail(const char *description)
{
	printf("FAIL: %s\n", description);
	record_failure();
}

static
void
fail_errno(const char *description, int expected_errno, int actual_errno)
{
	printf("FAIL: %s (expected errno %d, got %d)\n", description, expected_errno, actual_errno);
	record_failure();
}

static
void
expect_errno(const char *description, int syscall_failed, int expected_errno, int actual_errno)
{
	if (syscall_failed && actual_errno == expected_errno) {
		pass(description);
	}
	else {
		fail_errno(description, expected_errno, actual_errno);
	}
}

static
int
finish_test(const char *test_name)
{
	if (failures == 0) {
		printf("%s: PASS\n", test_name);
		return 0;
	}

	printf("%s: FAIL (%d checks failed)\n", test_name, failures);
	return 1;
}

#endif