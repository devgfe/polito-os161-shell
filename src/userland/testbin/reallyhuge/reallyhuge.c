/*
 * reallyhuge.c
 *
 * Deterministic ENOMEM helper for execv tests.
 *
 * This program declares a very large global BSS object so loading it
 * should fail with ENOMEM on small-memory kernels (e.g., sys161 with
 * ramsize=16M and dumbvm), before entering main().
 */

#include <stdio.h>

/* 24 MiB BSS: intentionally larger than common 16 MiB RAM setups. */
static char huge_bss[24 * 1024 * 1024];

int
main(void)
{
	/*
	 * If this message appears, execv did NOT fail with ENOMEM for this
	 * configuration.
	 */
	huge_bss[0] = 'X';
	huge_bss[sizeof(huge_bss) - 1] = 'Y';
	printf("reallyhuge started (size=%u bytes)\n", (unsigned)sizeof(huge_bss));
	return 0;
}