#!/usr/bin/env python3
#
# extra_tests.py - run additional tests that exercise the
# syscalls implemented for the shell assignment.

import sys
import os

# So we can "import runtest" no matter where we are launched from.
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import runtest

# Each entry is a kernel-menu command string.
TESTS = [
	# --- argument passing / execv ---
	"p /testbin/argtest a b c",		# prints argc/argv
	"p /testbin/add 3 8",			# needs execv args
	"p /testbin/factorial 5",		# recursive execv with args
	"p /testbin/bigexec",			# execv with many/large args

	# --- fork / waitpid / getpid / _exit ---
	"p /testbin/forktest",			# classic fork test
	"p /testbin/bigfork",			# heavier fork test
	"p /testbin/faulter",			# should die cleanly on bad access
	"p /testbin/farm",			    # fork+execv+waitpid in parallel

	# --- file I/O: open/read/write/close/lseek ---
	"p /testbin/bigfile /bigfile.out 1024",	  # create & write a file
	"p /testbin/sparsefile /sparse.out 4096", # lseek past EOF + write
	"p /testbin/hash /bigfile.out",		      # read a file byte by byte
	"p /testbin/tail /bigfile.out 512",	      # lseek + read + write

	# --- dup2 / redirect ---
	"p /testbin/redirect",			# dup2 + fork + execv + waitpid

	# --- badcall: invalid arguments for implemented syscalls ---
	# Run only the groups that map to syscalls we have:
	#   a=execv b=waitpid c=open d=read e=write f=close g=reboot
	#   j=lseek s=chdir w=dup2 z=__getcwd
	"p /testbin/badcall a b c d e f g j s w z",
]


def main():
	# One command per test, separated by ';' as runtest.run() expects.
	commands = ";".join(TESTS)

	# runtest.run() echoes all kernel output to stdout.
	msg = runtest.run(commands, sys.stdout.buffer)
	if msg is not None:
		print("\nRUN ABORTED: %s" % msg)
		return 1

	print("\nRun completed.")
	return 0


if __name__ == "__main__":
	sys.exit(main())
