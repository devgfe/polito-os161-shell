# Manual Test Programs

1. Create `src/userland/testbin/mytest/` with `mytest.c` and this `Makefile`:

   ```make
   TOP=../../..
   .include "$(TOP)/mk/os161.config.mk"

   PROG=mytest
   SRCS=mytest.c
   BINDIR=/testbin

   .include "$(TOP)/mk/os161.prog.mk"
   ```

2. Add `mytest` to `SUBDIRS` in the parent `Makefile` if it must be included in full userland builds.

3. Build and install the test:

   ```sh
   cd src/userland/testbin/mytest
   bmake
   bmake install
   ```

4. Start OS/161 and run it from the kernel menu:

   ```text
   p /testbin/mytest [arguments]
   ```