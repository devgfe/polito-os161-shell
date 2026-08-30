#!/usr/bin/env python3
#
# syscall_tests.py - run all the non-interactive syscall tests.
#
# Boots OS/161 and runs each test from the kernel menu, waiting for the
# prompt between commands.

import sys
import os

# So we can "import runtest" no matter where we are launched from.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import runtest

TESTS = [
	"chdirtest",
	"closetest",
	"dup2test",
	"execvtest",
	"getcwdtest",
	"getpidtest",
	"lseektest",
	"opentest",
	"readtest",
	"waitpidtest",
	"writetest",
	"zombietest",
]


def main():
	# One command per test, separated by ';' as runtest.run() expects.
	commands = ";".join("p /testbin/%s" % t for t in TESTS)

	# runtest.run() echoes all kernel output to stdout.
	msg = runtest.run(commands, sys.stdout.buffer)
	if msg is not None:
		print("\nRUN ABORTED: %s" % msg)
		return 1

	print("\nRun completed. Check the '[TEST-OK] <name>' / '[TEST-FAIL] <name>' lines above.")
	return 0


if __name__ == "__main__":
	sys.exit(main())
