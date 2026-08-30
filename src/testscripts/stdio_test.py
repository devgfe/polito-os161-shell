#!/usr/bin/env python3
#
# stdio_test.py - run stdiodtest.
#
# stdiodtest writes to stdout/stderr, then blocks reading ONE character
# from stdin. We boot OS/161, launch it, wait for its input prompt and
# answer with one character so it can finish unattended.

import sys
import pexpect

MENU_PROMPT = "OS/161 kernel [? for menu]: "
STDIO_PROMPT = "stdiodtest: type one character for stdin (fd 0): "
EXPECTED_CHAR = "x"   # must match EXPECTED_CHAR in stdiodtest.c


def main():
	# -X: exit instead of waiting for a debugger when the kernel halts.
	proc = pexpect.spawn("sys161", ["-X", "kernel"], timeout=60)
	proc.logfile_read = sys.stdout.buffer   # echo kernel output to stdout

	# 1. Wait for the kernel menu and launch stdiodtest.
	if proc.expect_exact([MENU_PROMPT, pexpect.EOF, pexpect.TIMEOUT]) != 0:
		print("\nABORTED: kernel menu never appeared")
		return 1
	proc.send("p /testbin/stdiodtest\r")

	# 2. Wait for stdiodtest to ask for input, then send one character.
	if proc.expect_exact([STDIO_PROMPT, pexpect.EOF, pexpect.TIMEOUT]) != 0:
		print("\nABORTED: stdiodtest input prompt never appeared")
		return 1
	proc.send(EXPECTED_CHAR)

	# 3. Wait until stdiodtest finishes (back to the menu), then quit.
	if proc.expect_exact([MENU_PROMPT, pexpect.EOF, pexpect.TIMEOUT]) != 0:
		print("\nABORTED: stdiodtest did not return to the menu")
		return 1
	proc.send("q\r")
	proc.expect_exact([pexpect.EOF, pexpect.TIMEOUT])

	print("\nDone. Check the '[TEST-OK] stdiodtest' / '[TEST-FAIL] stdiodtest' line above.")
	return 0


if __name__ == "__main__":
	sys.exit(main())
