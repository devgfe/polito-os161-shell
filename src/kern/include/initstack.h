/*
 * Shared helper for building the initial user-level
 * stack used by both runprogram() and sys_execv().
 */
#ifndef _INITSTACK_H_
#define _INITSTACK_H_

#include "opt-shell.h"

#if OPT_SHELL
#include <types.h>

/*
 * initstack - build the initial user stack layout with
 * the argv pointer vector and the string contents.
 *
 *   high addresses
 *   +---------------------+  <- stackptr (from as_define_stack)
 *   | argv strings        |  NUL-terminated, packed back to back
 *   +---------------------+
 *   | argv pointer vector |  argv[0..argc-1], then argv[argc] = NULL
 *   +---------------------+  <- new stack pointer (8-byte aligned)
 *   low addresses
 *
 * Returns in *ret_argv the user-space address of the argv vector,
 * which doubles as the new stack pointer for enter_new_process().
 */
int initstack(char **kargv, int argc, vaddr_t stackptr, userptr_t *ret_argv);

#endif /* OPT_SHELL */
#endif /* _INITSTACK_H_ */
