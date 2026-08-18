# polito-os161-shell

## Project C2 Design Document

This document summarizes the design and implementation choices for the OS/161 Shell project.

## System Calls

### `open`



### `read`



### `write`



### `lseek`



### `close`



### `dup2`



### `chdir`



### `__getcwd`



### `getpid`



### `fork`



### `execv`

`int execv(const char *program, char **args);` replaces the current address space but preserves the current process object, PID, current working directory, and FD table. This matches the OS/161 `execv` contract and is the key difference from `runprogram()`: `runprogram()` starts a fresh process from a kernel pathname, whereas `execv` starts from untrusted user pointers and must preserve process-scoped state.

1. `sys_execv` allocates a `PATH_MAX` pathname buffer and an `ARG_MAX` argument buffer. It copies the pathname with `copyinstr`, reads every pointer in the null-terminated `argv` vector with `copyin`, and copies each argument string with `copyinstr`. The `ARG_MAX` limit accounts for both the argument strings and the `argv` pointer vector; requests that exceed this space return `E2BIG`.
2. It opens the executable read-only, creates a new address space, installs and activates it, loads the ELF image, and defines the user stack. The old address space remains available in a local variable for rollback.
3. It places the argument strings just below the stack top, aligning the base of the string area down to a 4-byte boundary. Below the strings it reserves the user-space `argv` vector (`argc` pointers plus the terminating `NULL`), aligning its base down to an 8-byte boundary. All strings and pointers are written to these aligned addresses, and the 8-byte-aligned vector base doubles as the final stack pointer passed to `enter_new_process`, as required by the MIPS ABI. The old address space is destroyed only after the new stack has been fully built.

The new address space must be temporarily installed because `load_elf` loads the executable into the current address space. If loading the ELF image, creating the stack, or copying the arguments fails, `sys_execv` restores and reactivates the old address space, then destroys the incomplete new one. It also closes the executable vnode, if it is still open, and frees all temporary kernel buffers. On success, `execv` transfers control to the new program and does not return to its caller.

### `waitpid`



### `_exit`



## Other Modifications

### Standard File Descriptors for `runprogram` Processes

Each newly started user process is initialized with the following standard descriptors connected to the console device `con:`:

| FD | Meaning | Macro |
| --- | --- | --- |
| `0` | standard input (stdin) | `STDIN_FILENO` |
| `1` | standard output (stdout) | `STDOUT_FILENO` |
| `2` | standard error (stderr) | `STDERR_FILENO` |

The current branch adds `p_fdtable` to `struct proc`. `proc_create` initializes it to `NULL`; `proc_create_runprogram` calls `fdtable_create_standard()` after creating the process; and `proc_destroy` calls `fdtable_destroy()` before releasing the process. If standard-table creation fails, `proc_create_runprogram` destroys the partially created process and reports failure.

The standard FD table is initialized in `proc_create_runprogram` because it is part of the process-specific state created with each process.

## Tests

The following tests validate the implemented behaviour and relevant error paths.

### `execvtest`

`execvtest` validates `execv` in isolated child processes. For each case, the child calls `execv`; if it returns, the child exits with the observed errno and the parent compares that status after `waitpid`. The successful case executes `/testbin/argtest` with multiple arguments, which checks `argc`, every `argv[i]`, and the terminating null pointer. Error cases cover unknown devices (`ENODEV`), invalid path components (`ENOTDIR`), missing targets (`ENOENT`), directories (`EISDIR`), non-ELF files (`ENOEXEC`), oversized argument blocks (`E2BIG`), insufficient memory for a huge program (`ENOMEM`), a NULL program pointer (`EFAULT`), an invalid program pointer (`EFAULT`), a NULL `argv` pointer (`EFAULT`), an invalid `argv` pointer (`EFAULT`), and an invalid argument string pointer (`EFAULT`).

### `stdiodtest`

`stdiodtest` verifies that a newly started program can use its standard descriptors. It writes a message to FD `1`, writes a separate message to FD `2`, writes a prompt to FD `1`, reads one character from FD `0`, echoes that character to FD `1`, and writes a final success message. Each operation is reported independently. It depends on the `read` and `write` syscalls.

### `opentest`

`opentest` creates a read/write file at `/testbin/opentest.tmp` with `O_RDWR | O_CREAT | O_TRUNC`. Error cases cover unknown devices (`ENODEV`), regular files in the path (`ENOTDIR`), missing files (`ENOENT`), `O_CREAT | O_EXCL` on an existing file (`EEXIST`), directories opened for writing (`EISDIR`), invalid flags (`EINVAL`), a NULL pathname (`EFAULT`), and an invalid pathname pointer (`EFAULT`).

### `closetest`

`closetest` opens `/testbin/closetest.tmp` with `O_RDONLY | O_CREAT`, closes the descriptor successfully, and verifies that closing the same descriptor again returns `EBADF`. Also checks `close(-1)` (`EBADF`).

### `readtest`

`readtest` prepares `/testbin/readtest.tmp` with known content, reads it back, and checks that `read` returns the full requested byte count with matching data, and that a subsequent read returns zero. Error cases cover invalid descriptors (`EBADF`), write-only descriptors (`EBADF`), and invalid buffer pointers (`EFAULT`).

### `writetest`

`writetest` writes known content to `/testbin/writetest.tmp`, reopens the file for reading, and verifies the stored data matches. Error cases cover invalid descriptors (`EBADF`), read-only descriptors (`EBADF`), and invalid buffer pointers (`EFAULT`).

### `lseektest`

`lseektest` exercises all three whence modes on a file containing the six-byte string `"abcdef"`. `SEEK_SET` seeks to offset `2` and reads `'c'`; `SEEK_CUR` advances by `1` from the current position and reads `'e'`; `SEEK_END` jumps past the last byte and confirms that a subsequent read returns zero. Error cases cover invalid descriptors (`EBADF`), negative resulting offsets (`EINVAL`), invalid whence values (`EINVAL`), and non-seekable devices (`null:`, `ESPIPE`).

### `dup2test`

`dup2test` duplicates a descriptor opened on `/testbin/dup2test.tmp` and verifies that `dup2` returns the target descriptor and that the original and the duplicate share the seek offset. Tests that `dup2` onto itself preserves the descriptor. Error cases cover invalid source descriptors (`EBADF`), invalid target descriptors (`EBADF`), and impossible target descriptors (`EBADF`).

### `chdirtest`

`chdirtest` changes to `/testbin` and confirms the result by opening `argtest` with a relative path, then changes back to `/`. Error cases cover unknown devices (`ENODEV`), regular files in the path (`ENOTDIR`), missing paths (`ENOENT`), a NULL pathname (`EFAULT`), and an invalid pathname pointer (`EFAULT`).

### `getcwdtest`

`getcwdtest` changes to `/testbin`, calls `__getcwd` with a sufficiently sized buffer, and verifies that the returned length and content match the expected directory. Error cases cover a NULL buffer (`EFAULT`) and an invalid buffer pointer (`EFAULT`).

### `getpidtest`

`getpidtest` calls `getpid` twice and checks that the returned value is positive and stable across successive calls within the same process.

### `waitpidtest`

`waitpidtest` forks a child that calls `_exit` with a known status, confirms that `waitpid` returns the child PID and correctly reports the exit status. Verifies that reaping the same child again returns `ESRCH`, that a NULL status pointer is accepted, that an invalid status pointer returns `EFAULT`, that unsupported `options` flags return `EINVAL`, and that calling `waitpid` on the calling process's own PID returns `ECHILD`.

## References

- [OS/161 Manual](https://people.ece.ubc.ca/os161/man/)
- [Linux Man Pages](https://man7.org/linux/man-pages/index.html)