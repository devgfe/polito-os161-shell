# Changelog

> This document shows the difference between the **final state** of the `main` branch and the original OS/161 baseline commit `c992cf9`: it lists every file that was **added or modified** during the project work.

**Summary** — 67 files differ from the baseline on `main`: **45 added (+)** and **22 modified (~)**.

---

## 🌳 File tree

<pre>
os161/
│
├── <a href="README.md">📄 README.md</a>                              (~)
├── <a href="ASSIGNMENT.md">📄 ASSIGNMENT.md</a>                          (+)
├── <a href="CHANGELOG.md">📄 CHANGELOG.md</a> *                         (+)
├── <a href="CONTRIBUTING.md">📄 CONTRIBUTING.md</a>                        (+)
├── <a href="CONTRIBUTORS.md">📄 CONTRIBUTORS.md</a>                        (+)
├── <a href="INSTALL.md">📄 INSTALL.md</a>                              (+)
├── <a href=".gitattributes">📄 .gitattributes</a>                          (+)
├── <a href=".gitignore">📄 .gitignore</a>                              (~)
│
├── 📁 .vscode/
│   ├── <a href=".vscode/tasks.json">📄 tasks.json</a>                          (~)
│   └── <a href=".vscode/c_cpp_properties.json">📄 c_cpp_properties.json</a>              (~)
│
├── 📁 root/
│   ├── <a href="root/sys161.conf">📄 sys161.conf</a>                          (~)
│   └── <a href="root/.gdbinit">📄 .gdbinit</a>                              (+)
│
└── 📁 src/
    ├── 📁 kern/
    │   ├── 📁 conf/
    │   │   ├── <a href="src/kern/conf/conf.kern">📄 conf.kern</a>                      (~)
    │   │   └── <a href="src/kern/conf/SHELL">📄 SHELL</a>                          (+)
    │   ├── 📁 include/
    │   │   ├── <a href="src/kern/include/proc.h">📄 proc.h</a>                          (~)
    │   │   ├── <a href="src/kern/include/syscall.h">📄 syscall.h</a>                        (~)
    │   │   ├── <a href="src/kern/include/synch.h">📄 synch.h</a>                          (~)
    │   │   ├── <a href="src/kern/include/test.h">📄 test.h</a>                           (~)
    │   │   ├── <a href="src/kern/include/emufs.h">📄 emufs.h</a>                          (~)
    │   │   ├── <a href="src/kern/include/filetable.h">📄 filetable.h</a>                      (+)
    │   │   └── <a href="src/kern/include/initstack.h">📄 initstack.h</a>                      (+)
    │   ├── 📁 filetable/
    │   │   └── <a href="src/kern/filetable/filetable.c">📄 filetable.c</a>                  (+)
    │   ├── 📁 syscall/
    │   │   ├── <a href="src/kern/syscall/file_syscalls.c">📄 file_syscalls.c</a>                (+)
    │   │   ├── <a href="src/kern/syscall/proc_syscalls.c">📄 proc_syscalls.c</a>                (+)
    │   │   ├── <a href="src/kern/syscall/runprogram.c">📄 runprogram.c</a>                    (~)
    │   │   └── <a href="src/kern/syscall/initstack.c">📄 initstack.c</a>                      (+)
    │   ├── 📁 proc/
    │   │   └── <a href="src/kern/proc/proc.c">📄 proc.c</a>                            (~)
    │   ├── 📁 thread/
    │   │   ├── <a href="src/kern/thread/thread.c">📄 thread.c</a>                          (~)
    │   │   └── <a href="src/kern/thread/synch.c">📄 synch.c</a>                            (~)
    │   ├── 📁 arch/mips/
    │   │   ├── 📁 locore/
    │   │   │   └── <a href="src/kern/arch/mips/locore/trap.c">📄 trap.c</a>                        (~)
    │   │   ├── 📁 syscall/
    │   │   │   └── <a href="src/kern/arch/mips/syscall/syscall.c">📄 syscall.c</a>                  (~)
    │   │   └── 📁 vm/
    │   │       └── <a href="src/kern/arch/mips/vm/dumbvm.c">📄 dumbvm.c</a>                      (~)
    │   ├── 📁 dev/lamebus/
    │   │   └── <a href="src/kern/dev/lamebus/emu.c">📄 emu.c</a>                            (~)
    │   └── 📁 main/
    │       ├── <a href="src/kern/main/main.c">📄 main.c</a>                            (~)
    │       └── <a href="src/kern/main/menu.c">📄 menu.c</a>                            (~)
    │
    ├── 📁 testscripts/
    │   ├── <a href="src/testscripts/shell_tests.py">📄 shell_tests.py</a>                    (+)
    │   └── <a href="src/testscripts/extra_tests.py">📄 extra_tests.py</a>                    (+)
    │
    └── 📁 userland/testbin/
        ├── <a href="src/userland/testbin/Makefile">📄 Makefile</a>                          (~)
        ├── <a href="src/userland/testbin/README.md">📄 README.md</a>                        (+)
        ├── <a href="src/userland/testbin/testreport.h">📄 testreport.h</a>                    (+)
        ├── 📁 chdirtest/    <a href="src/userland/testbin/chdirtest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/chdirtest/chdirtest.c">📄 chdirtest.c</a>      (+)
        ├── 📁 closetest/    <a href="src/userland/testbin/closetest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/closetest/closetest.c">📄 closetest.c</a>      (+)
        ├── 📁 dup2test/     <a href="src/userland/testbin/dup2test/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/dup2test/dup2test.c">📄 dup2test.c</a>        (+)
        ├── 📁 execvtest/    <a href="src/userland/testbin/execvtest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/execvtest/execvtest.c">📄 execvtest.c</a>      (+)
        ├── 📁 getcwdtest/   <a href="src/userland/testbin/getcwdtest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/getcwdtest/getcwdtest.c">📄 getcwdtest.c</a>    (+)
        ├── 📁 getpidtest/   <a href="src/userland/testbin/getpidtest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/getpidtest/getpidtest.c">📄 getpidtest.c</a>    (+)
        ├── 📁 lseektest/    <a href="src/userland/testbin/lseektest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/lseektest/lseektest.c">📄 lseektest.c</a>      (+)
        ├── 📁 opentest/     <a href="src/userland/testbin/opentest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/opentest/opentest.c">📄 opentest.c</a>        (+)
        ├── 📁 readtest/     <a href="src/userland/testbin/readtest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/readtest/readtest.c">📄 readtest.c</a>        (+)
        ├── 📁 reallyhuge/   <a href="src/userland/testbin/reallyhuge/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/reallyhuge/reallyhuge.c">📄 reallyhuge.c</a>    (+)
        ├── 📁 stdiodtest/   <a href="src/userland/testbin/stdiodtest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/stdiodtest/stdiodtest.c">📄 stdiodtest.c</a>    (+)
        ├── 📁 waitpidtest/  <a href="src/userland/testbin/waitpidtest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/waitpidtest/waitpidtest.c">📄 waitpidtest.c</a>  (+)
        ├── 📁 writetest/    <a href="src/userland/testbin/writetest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/writetest/writetest.c">📄 writetest.c</a>      (+)
        └── 📁 zombietest/   <a href="src/userland/testbin/zombietest/Makefile">📄 Makefile</a>, <a href="src/userland/testbin/zombietest/zombietest.c">📄 zombietest.c</a>    (+)
</pre>