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
3. It places the argument strings below the stack top; strings may start at any byte address and therefore require no alignment. It then rounds the address below the strings down before allocating the user-space `argv` vector. Because each pointer is four bytes, the vector starts at a 4-byte-aligned address; after writing `argv[argc] = NULL`, the final stack pointer is rounded down to an 8-byte boundary before destroying the old address space and calling `enter_new_process`.

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

`execvtest` validates `execv` in isolated child processes. For each case, the child calls `execv`; if it returns, the child exits with the observed errno and the parent compares that status after `waitpid`. The successful case executes `/testbin/argtest` with multiple arguments, which checks `argc`, every `argv[i]`, and the terminating null pointer. Error cases cover unknown devices (`ENODEV`), invalid path components (`ENOTDIR`), missing targets (`ENOENT`), directories (`EISDIR`), non-ELF files (`ENOEXEC`), oversized argument blocks (`E2BIG`), invalid user pointers (`EFAULT`), and insufficient memory (`ENOMEM`).

### `stdiodtest`

`stdiodtest` verifies that a newly started program can use its standard descriptors. It writes a message to FD `1`, writes a separate message to FD `2`, reads one character from FD `0`, echoes that character to FD `1`, and returns a distinct failure status for each stage. It depends on the `read` and `write` syscalls.

## References

- [OS/161 Manual](https://people.ece.ubc.ca/os161/man/)
- [Linux Man Pages](https://man7.org/linux/man-pages/index.html)