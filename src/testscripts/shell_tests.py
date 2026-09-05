#!/usr/bin/env python3
#
# shell_tests.py - run all shell tests.
#
# Boots OS/161 once and runs every test from the kernel menu, waiting for
# the prompt between commands. Two kinds of tests are supported:
#
#   - non-interactive ("run"):     just send the command and wait for the
#                                  kernel menu to come back.
#   - interactive     ("run_tty"): send the command, wait for a
#                                  test-specific input prompt, send the
#                                  reply, then wait for the kernel menu.
#
# Each entry in TESTS is a tuple with the arguments for one of the two
# functions above (the function name first).

import sys
import pexpect

MENU_PROMPT = "OS/161 kernel [? for menu]: "


#
# Wait for the kernel menu prompt.
# Returns None on success, or an error message on panic/EOF/timeout.
#
def wait_menu(proc, what):
	which = proc.expect_exact([
		MENU_PROMPT,
		"panic: ",		# kernel panic
		pexpect.EOF,
		pexpect.TIMEOUT,
	])
	if which == 0:
		return None
	if which == 1:
		return "%s: kernel panic" % what
	if which == 2:
		return "%s: unexpected end of input" % what
	return "%s: timeout waiting for the kernel menu" % what


#
# Non-interactive test: send the command, wait for the menu to return.
#
def run(proc, command):
	proc.send("%s\r" % command)
	return wait_menu(proc, command)


#
# Interactive test: send the command, wait for the test's input prompt,
# send the reply, then wait for the menu to return.
#
def run_tty(proc, command, prompt, reply):
	proc.send("%s\r" % command)
	which = proc.expect_exact([
		prompt,
		"panic: ",
		pexpect.EOF,
		pexpect.TIMEOUT,
	])
	if which == 1:
		return "%s: kernel panic" % command
	if which == 2:
		return "%s: unexpected end of input" % command
	if which == 3:
		return "%s: input prompt never appeared" % command
	proc.send(reply)
	return wait_menu(proc, command)


#
# The test list: one tuple per test, function name first.
#
TESTS = [
	# --- non-interactive syscall tests ---
	(run,      "p /testbin/chdirtest"),
	(run,      "p /testbin/closetest"),
	(run,      "p /testbin/dup2test"),
	(run,      "p /testbin/execvtest"),
	(run,      "p /testbin/getcwdtest"),
	(run,      "p /testbin/getpidtest"),
	(run,      "p /testbin/lseektest"),
	(run,      "p /testbin/opentest"),
	(run,      "p /testbin/readtest"),
	(run,      "p /testbin/waitpidtest"),
	(run,      "p /testbin/writetest"),
	(run,      "p /testbin/zombietest"),

	# --- interactive tests ---
	(run_tty,  "p /testbin/stdiodtest", "stdiodtest: type one character for stdin (fd 0): ", "x"),
]


def main():
	# -X: exit instead of waiting for a debugger when the kernel halts.
	proc = pexpect.spawn("sys161", ["-X", "kernel"], timeout=180)
	proc.logfile_read = sys.stdout.buffer   # echo kernel output to stdout

	# Wait for the first kernel menu after boot.
	msg = wait_menu(proc, "boot")
	if msg is not None:
		print("\nRUN ABORTED: %s" % msg)
		return 1

	# Run every test in order.
	for entry in TESTS:
		func, args = entry[0], entry[1:]
		msg = func(proc, *args)
		if msg is not None:
			print("\nRUN ABORTED: %s" % msg)
			proc.send("q\r")
			proc.expect_exact([pexpect.EOF, pexpect.TIMEOUT])
			return 1

	# All done: shut down the kernel.
	proc.send("q\r")
	proc.expect_exact([pexpect.EOF, pexpect.TIMEOUT])

	print("\nRun completed. Check the '[TEST-OK] <name>' / '[TEST-FAIL] <name>' lines above.")
	return 0


if __name__ == "__main__":
	sys.exit(main())
