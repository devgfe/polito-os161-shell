#
# Makefile for src/testbin (sources for programs installed in /testbin)
#

TOP=../..
.include "$(TOP)/mk/os161.config.mk"

SUBDIRS=add argtest badcall bigexec bigfile bigfork bigseek bloat chdirtest closetest conman \
	crash ctest dirconc dirseek dirtest dup2test execvtest f_test factorial farm faulter \
	filetest forkbomb forktest frack getcwdtest getpidtest hash hog huge lseektest \
	malloctest matmult multiexec opentest palin parallelvm poisondisk psort readtest \
	randcall redirect rmdirtest rmtest reallyhuge \
	sbrktest schedpong sort sparsefile stdiodtest tail tictac triplehuge \
	triplemat triplesort usemtest waitpidtest writetest zero zombietest

# But not:
#    userthreads    (no support in kernel API in base system)

.include "$(TOP)/mk/os161.subdir.mk"
