/* Shared reporting helpers for standalone userland tests. */

#ifndef TESTBIN_TESTREPORT_H
#define TESTBIN_TESTREPORT_H

#include <stdio.h>

/*
 * Every helper below is static, so each test that includes this header
 * gets its own copy. A given test only uses a subset of the helpers,
 * and the build passes -Werror -Wunused-function, so we mark them all
 * __attribute__((__unused__)) to keep the unused ones from failing the
 * build. The attribute only suppresses the warning; the functions are
 * still compiled and usable.
 */
#define TESTHELPER static __attribute__((__unused__))

static int failures;

TESTHELPER
void
record_failure(void)
{
	failures++;
}

TESTHELPER
void
pass(const char *description)
{
	printf("PASS: %s\n", description);
}

TESTHELPER
void
fail(const char *description)
{
	printf("FAIL: %s\n", description);
	record_failure();
}

TESTHELPER
void
fail_errno(const char *description, int expected_errno, int actual_errno)
{
	printf("FAIL: %s (expected errno %d, got %d)\n", description, expected_errno, actual_errno);
	record_failure();
}

TESTHELPER
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

TESTHELPER
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