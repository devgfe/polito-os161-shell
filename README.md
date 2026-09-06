# [polito-os161-shell](https://github.com/devgfe/polito-os161-shell)

## Project C2 Design Document

This document summarizes the design and implementation choices for the OS/161 Shell project.

## System Calls

### `open`

`int open(const char *filename, int flags, ...);` opens (optionally creating) the file at `filename` and returns a new file descriptor referring to it, or `-1` on error.

1. `sys_open` rejects any flag bits outside `O_ACCMODE | O_CREAT | O_EXCL | O_TRUNC | O_APPEND` with `EINVAL`, then copies the pathname in with `copyinstr`.
2. It calls `fdtable_open` (see [The File Descriptor Table](#the-file-descriptor-table) below), which opens the vnode with `vfs_open`, allocates a system-wide `open_file` entry for it, and inserts it into the first free slot of the calling process's FD table.

### `read`

`ssize_t read(int fd, void *buf, size_t buflen);` reads up to `buflen` bytes from `fd` into `buf`, returning the number of bytes actually read (`0` at end of file).

1. `sys_read` allocates a kernel buffer of size `buflen`.
2. It calls `fdtable_read`, which looks up the descriptor's `open_file`, rejects descriptors not opened for reading (`EBADF`), and performs the transfer with `VOP_READ` through a `uio` built from the current file offset, advancing the offset by the number of bytes actually transferred.
3. It copies the bytes actually read back to the user buffer with `copyout`, but only when at least one byte was transferred — a zero-length `copyout` spuriously returns `EFAULT` on this base distribution (`copycheck` underflows when computing the end address of a zero-length region), which would otherwise make every end-of-file read fail.

### `write`

`ssize_t write(int fd, const void *buf, size_t nbytes);` writes up to `nbytes` bytes from `buf` to `fd`, returning the number of bytes actually written.

1. `sys_write` copies the user buffer into a kernel buffer with `copyin`.
2. It calls `fdtable_write`, which rejects descriptors not opened for writing (`EBADF`) and performs the transfer with `VOP_WRITE` through a `uio` built from the current file offset, advancing the offset by the number of bytes actually written.

### `lseek`

`off_t lseek(int fd, off_t pos, int whence);` repositions the file offset of `fd` according to `whence` (`SEEK_SET`, `SEEK_CUR`, `SEEK_END`) and returns the resulting offset.

1. Because `off_t` is 64 bits, the MIPS syscall convention passes `fd` in `a0`, leaves `a1` unused for register-pair alignment, and packs `pos` across `a2`/`a3`. `whence` no longer fits in a register and is read from the user stack at `sp+16` via `copyin` in the dispatcher, which also splits the 64-bit return value across `v0`/`v1`.
2. `fdtable_lseek` computes the new offset for each `whence` case, querying the file size with `VOP_STAT` for `SEEK_END`.
3. It rejects the call if the descriptor isn't seekable (`VOP_ISSEEKABLE`, `ESPIPE`) or if the resulting offset would be negative (`EINVAL`).

### `close`

`int close(int fd);` closes a file descriptor.

1. `sys_close` forwards to `fdtable_close`, which removes the descriptor from the process's FD table.
2. It decrements the reference count of the underlying `open_file`; the vnode is only actually closed with `vfs_close` once that count reaches zero, since the same `open_file` may still be referenced by another descriptor created with `dup2` or inherited through `fork` (see [The File Descriptor Table](#the-file-descriptor-table)).

### `dup2`

`int dup2(int oldfd, int newfd);` makes `newfd` refer to the same open file as `oldfd`.

1. `fdtable_dup2` increments the reference count of `oldfd`'s `open_file`.
2. It releases whatever was previously open on `newfd` first, following the same reference-counted teardown as `fdtable_close`, then installs the shared `open_file` at `newfd`.
3. `dup2(fd, fd)` is a no-op that simply returns `fd`.

### `chdir`

`int chdir(const char *pathname);` changes the calling process's current working directory.

1. Unlike the other calls, the current working directory is not tracked in the FD table: `sys_chdir` copies the pathname in with `copyinstr`.
2. It forwards the pathname directly to `vfs_chdir`, the base-distribution function that updates `curproc->p_cwd`.

### `__getcwd`

`ssize_t __getcwd(char *buf, size_t buflen);` retrieves the calling process's current working directory as a string.

1. `sys___getcwd` builds a `uio` targeting the user-supplied buffer and calls `vfs_getcwd`.
2. `vfs_getcwd` relies on `VOP_NAMEFILE`, which in the base OS/161 distribution is only implemented for the root directory of a volume on both EMUFS and SFS. This project's own `emufs_namefile` patch (see [EMUFS Path Tracking for `__getcwd`](#emufs-path-tracking-for-__getcwd) below) lifts that restriction for EMUFS, so `__getcwd` works from any EMUFS subdirectory; on SFS the restriction is unchanged, and calling `__getcwd` from an SFS subdirectory still fails regardless of the caller — a base-distribution limitation, not specific to this implementation.

### `getpid`

`pid_t getpid(void);` returns the calling process's PID. `sys_getpid` simply reads `curproc->p_pid`, which is assigned once when the process is created (see [PID Assignment and the Process Table](#pid-assignment-and-the-process-table) below) and never changes afterwards, so the call cannot fail.

### `fork`

`pid_t fork(void);` creates a new process that is a copy of the calling process, duplicating its address space and file descriptor table. On success, the call returns the child's PID in the parent and `0` in the child; on failure it returns `-1` and sets `errno` in the parent, and no child is created.

1. `sys_fork` copies the parent's trapframe onto the heap, since the child resumes execution through it after `thread_fork` returns to the parent (the on-stack trapframe of `sys_fork`'s caller would otherwise be gone by the time the child runs).
2. It creates the child process with `proc_create_runprogram`, which assigns a new PID and records the parent's PID in `p_parent`; the child's `p_fdtable` is left `NULL` at this point (see [Standard File Descriptors for `runprogram` Processes](#standard-file-descriptors-for-runprogram-processes)) and is only populated by the next step.
3. It duplicates the parent's address space with `as_copy` and replaces the child's FD table with a clone of the parent's via `fdtable_clone`, so the child inherits copies of every open file descriptor rather than sharing the parent's table.
4. It registers the child's PID in the parent's children list with `proc_add_child` (see [Process Tree: Parent/Child Tracking](#process-tree-parentchild-tracking)), then starts the child with `thread_fork`, passing `enter_forked_process` and the heap-allocated trapframe.
5. `enter_forked_process` (`src/kern/arch/mips/syscall/syscall.c`) runs on the new thread: it copies the trapframe onto its own stack, frees the heap copy, sets `tf_v0 = 0` and `tf_a3 = 0` so the child observes a `0` return value with no error, advances `tf_epc` past the `syscall` instruction, and enters user mode via `mips_usermode`.

If any step fails, `sys_fork` tears down whatever was already allocated (the child process, the trapframe copy, and, if registered, the child's entry in the parent's children list) and returns the error code; the parent's own state and trapframe are never modified, so it simply resumes with `fork` returning `-1` and the appropriate `errno`.

### `execv`

`int execv(const char *program, char **args);` replaces the current address space but preserves the current process object, PID, current working directory, and FD table. This matches the OS/161 `execv` contract and is the key difference from `runprogram()`: `runprogram()` starts a fresh process from a kernel pathname, whereas `execv` starts from untrusted user pointers and must preserve process-scoped state.

1. `sys_execv` allocates a `PATH_MAX` pathname buffer and an `ARG_MAX` argument buffer. It copies the pathname with `copyinstr`, reads every pointer in the null-terminated `argv` vector with `copyin`, and copies each argument string with `copyinstr`. The `ARG_MAX` limit accounts for both the argument strings and the `argv` pointer vector; requests that exceed this space return `E2BIG`.
2. It opens the executable read-only, creates a new address space, installs and activates it, loads the ELF image, and defines the user stack. The old address space remains available in a local variable for rollback.
3. It places the argument strings just below the stack top, aligning the base of the string area down to a 4-byte boundary. Below the strings it reserves the user-space `argv` vector (`argc` pointers plus the terminating `NULL`), aligning its base down to an 8-byte boundary. All strings and pointers are written to these aligned addresses, and the 8-byte-aligned vector base doubles as the final stack pointer passed to `enter_new_process`, as required by the MIPS ABI. The old address space is destroyed only after the new stack has been fully built.

The new address space must be temporarily installed because `load_elf` loads the executable into the current address space. If loading the ELF image, creating the stack, or copying the arguments fails, `sys_execv` restores and reactivates the old address space, then destroys the incomplete new one. It also closes the executable vnode, if it is still open, and frees all temporary kernel buffers. On success, `execv` transfers control to the new program and does not return to its caller.

### `waitpid`

`pid_t waitpid(pid_t pid, int *status, int options);` blocks the calling process until the specified child terminates, then reaps it and reports its exit status.

1. `sys_waitpid` accepts `options == 0` or `WNOHANG`; any other bit set returns `EINVAL`.
2. It looks up the target process with `proc_lookup_child(curproc, pid, &child)`, which returns `ESRCH` if no such PID exists or `ECHILD` if it exists but its recorded parent isn't the caller — checked atomically, under `pid_lock`, in the same critical section as the lookup (see [PID Assignment and the Process Table](#pid-assignment-and-the-process-table)). This means a process can only wait on its own direct children, and a PID that has already been reaped by a previous `waitpid` call fails with `ESRCH` because the PID no longer resolves to any process. Looking up the PID and checking parentage atomically (rather than as two separate steps) closes a use-after-free/TOCTOU hole: without it, the target PID could be reaped and reassigned to an unrelated process between the lookup and the parent check, and the parent check would then pass or fail against the wrong process.
3. With `WNOHANG`, if the child has not exited yet, it returns `0` immediately without blocking.
4. Otherwise it blocks in `proc_wait(child)` (see [Wait/Exit Synchronization](#waitexit-synchronization)) until the child calls `_exit`, then copies the exit status out to `status` (if non-NULL, via `copyout`), destroys the child's process structure with `proc_destroy`, and removes its entry from the caller's children list with `proc_remove_child`.

Because reaping happens inside `sys_waitpid` itself, a child that has exited but not yet been waited for is a *zombie*: its `struct proc` (PID, exit code) stays alive in the process table until a `waitpid` call — or, if the parent exits first, `proc_remove_all_children` — collects it. See [Process Tree: Parent/Child Tracking](#process-tree-parentchild-tracking) for how orphaned and already-exited children are reclaimed.

### `_exit`

`void _exit(int exitcode) __attribute__((__noreturn__));` terminates the calling process and makes `exitcode` (encoded with `_MKWAIT_EXIT`) available to a parent that calls `waitpid`; the call never returns.

`sys__exit` forwards directly to `proc_exit(_MKWAIT_EXIT(exitcode))`:

- `_MKWAIT_EXIT(exitcode)` (`src/kern/include/kern/wait.h`) encodes the raw exit code into the wait-status format `waitpid` expects: the value is shifted left two bits (`_MKWVAL`) and OR'd with `__WEXITED`, using the low two bits to tag *how* the process ended (as opposed to `__WSIGNALED`/`__WCORED`/`__WSTOPPED` for a process killed by a signal). `WIFEXITED`/`WEXITSTATUS` on the caller side decode it back into the plain exit code.
- `proc_exit` (`src/kern/proc/proc.c`, see [Wait/Exit Synchronization](#waitexit-synchronization) and [Process Tree: Parent/Child Tracking](#process-tree-parentchild-tracking) for the full picture) performs the actual termination: it detaches the current thread from `curproc`, reparents or reaps the process's own children via `proc_remove_all_children`, then — under `p_waitlock` together with `p_lock` — stores the encoded status in `p_exitcode`, sets `p_exited`, and signals `p_waitcv` to wake a parent blocked in `proc_wait`. If the process is itself already an orphan at this point, it destroys itself immediately with `proc_destroy`; otherwise it is left alive as a zombie for the parent to reap via `waitpid`. It finishes by calling `thread_exit()`, which does not return.

## Other Modifications

### EMUFS Path Tracking for `__getcwd`

To support `sys___getcwd` on `emufs`, the project adds `ev_path` to `struct emufs_vnode` in `src/kern/include/emufs.h`. `ev_path` stores the vnode path relative to the filesystem root (`""` for root, `"/..."` for non-root nodes).

In `src/kern/dev/lamebus/emu.c`, this path is propagated when creating/loading child vnodes (`parent ev_path + "/" + name`) and returned by `emufs_namefile`. This allows `vfs_getcwd` to compose the current working directory as `<volume name or device name>:` + vnode path, making `__getcwd` work correctly on `emufs`. The allocated `ev_path` is released during vnode reclaim.

### Standard File Descriptors for `runprogram` Processes

Each newly started user process is initialized with the following standard descriptors connected to the console device `con:`:

| FD | Meaning | Macro |
| --- | --- | --- |
| `0` | standard input (stdin) | `STDIN_FILENO` |
| `1` | standard output (stdout) | `STDOUT_FILENO` |
| `2` | standard error (stderr) | `STDERR_FILENO` |

The current branch adds `p_fdtable` to `struct proc`. `proc_create` initializes it to `NULL`; `common_prog` calls `fdtable_create_standard()` after creating the process; and `proc_destroy` calls `fdtable_destroy()` before releasing the process. If standard-table creation fails, `common_prog` destroys the partially created process and reports failure.

The standard FD table is initialized in `common_prog` because it is part of the process-specific state required by a program started from the kernel menu. Instead, `sys_fork` initializes the child's FD table with `fdtable_clone()`, so the child inherits the parent's descriptors.

### `struct proc` Additions

To support `getpid`, `fork`, `waitpid` and `_exit`, `struct proc` (`src/kern/include/proc.h`) gained the following fields, all guarded by `OPT_SHELL`:

| Field | Purpose |
| --- | --- |
| `p_pid` | This process's own PID, assigned once at creation and never reused while the process is alive. |
| `p_parent` | The PID of the parent process, or `NO_PARENT` if the process is a kernel process or has been orphaned. |
| `p_children` | A singly linked list (`struct proc_node`) of the PIDs of this process's still-registered children. |
| `p_exited` | Set to `true` when the process has run `_exit`; read by `waitpid`/`proc_wait` to decide whether to block. |
| `p_exitcode` | The encoded exit status (via `_MKWAIT_EXIT`), valid once `p_exited` is `true`. |
| `p_waitlock` / `p_waitcv` | A lock/condition-variable pair used to block a waiting parent until the process exits (see below). |
| `p_fdtable` | The process's file descriptor table (see [Standard File Descriptors for `runprogram` Processes](#standard-file-descriptors-for-runprogram-processes)). |

`proc_create` initializes all of these fields (PID fields to `NO_PID`/`NO_PARENT`, `p_children` to `NULL`, `p_exited` to `false`), then assigns a PID via `proc_assign_pid`, which is called unconditionally for every process, including the very first (the kernel process) — see [PID Assignment and the Process Table](#pid-assignment-and-the-process-table) for how it tells the two cases apart. `proc_destroy` tears them down in the opposite order it matters for safety: it releases the PID back into the table *first* (via `pid_release`), then destroys `p_waitcv`/`p_waitlock` and `p_fdtable`. It does not free `p_children`, since by the time `proc_destroy` runs — either from `sys_waitpid`, or from `proc_exit` on an orphan — the children list has already been cleared by `proc_remove_all_children`.

### PID Assignment and the Process Table

PIDs are managed with a fixed-size table, `process_table[PID_MAX + 1]` (`PID_MAX == 32767`), indexed directly by PID, plus a single `pid_lock` protecting both the table and the `next_pid` allocation cursor.

- `proc_assign_pid` is called from `proc_create` for *every* process, including the very first (the kernel process). It and `pid_release` share a `needlock = (kproc != NULL)` flag that tells them whether `pid_lock` already exists: while `kproc` is still `NULL` (i.e. while the kernel process itself is being created, this early in boot), both functions skip locking entirely, since no other thread can be contending for the table yet; for every process created afterward, they take `pid_lock` as usual.
- Under that lock (when held), `proc_assign_pid` calls `find_valid_pid`, which returns `next_pid` while PIDs have never wrapped around, or, once `next_pid` exceeds `PID_MAX`, falls back to a linear scan of `process_table` for the first free slot. This keeps allocation O(1) in the common case while still reclaiming PIDs freed by long-exited processes once the space is exhausted. Returns `ENPROC` if no PID is free.
- `proc_lookup(pid)` provides locked read access to `process_table`, but is no longer called anywhere in this project's code — `waitpid` and `proc_remove_all_children` instead use `proc_lookup_child(parent, pid, &out)`, which looks up the PID *and* verifies, in the same `pid_lock` critical section, that the process's recorded `p_parent` matches `parent` before handing back the pointer (`ESRCH` if the PID doesn't resolve, `ECHILD` if it does but isn't `parent`'s child). This atomicity is what prevents a stale or PID-recycled lookup from handing back a `struct proc*` that no longer belongs to the caller — see [Process Tree: Parent/Child Tracking](#process-tree-parentchild-tracking) for how it's used there. `pid_release(pid)` provides the matching locked clear access, used by `proc_destroy` freeing a slot.

Because `process_table` holds a raw pointer to the `struct proc`, a PID is only safe to reuse after `proc_destroy` has cleared its slot — this is what ties PID lifetime to process-structure lifetime, and is the reason zombie processes (exited but not yet reaped) still occupy a table slot.

### Process Tree: Parent/Child Tracking

Each process tracks its direct children in `p_children`, populated by `proc_add_child` (called from `sys_fork` right after the child is created) and consulted by `sys_waitpid` (via `proc_remove_child`, once a child has been reaped) to enforce that a process may only wait on its own children.

The trickier part is what happens when a process terminates while it still has live children or is itself still expected by a parent — this is handled by `proc_exit` (`src/kern/proc/proc.c`), called from `sys__exit`:

1. It first calls `proc_remove_all_children(p)`, which walks `p`'s children list once and, for each recorded child PID, re-validates it with the same `proc_lookup_child(p, pid, &child)` primitive `waitpid` uses (see [PID Assignment and the Process Table](#pid-assignment-and-the-process-table)) before touching it, then clears the child's `p_parent` back to `NO_PARENT` under the child's own `p_lock`. Re-validating through `proc_lookup_child` here, rather than indexing `process_table` directly, is what makes this safe even if a child PID in the list is stale: it confirms the process at that PID still exists *and* is still actually `p`'s child before the parent pointer is cleared. A child that has *already* exited (a zombie the parent never got around to waiting for) is reaped right there, with `proc_wait` plus `proc_destroy`; a child that is still running is simply orphaned — its `p_parent` is `NO_PARENT`, so it will later be reaped by its own `proc_exit` instead of by a `waitpid` call, since no process will ever be able to wait on it again.
2. It then records the exit code and sets `p_exited`, all under `p_waitlock` together with `p_lock`, and reads whether `p` itself was already orphaned (`p_parent == NO_PARENT`) in that same critical section. Doing the exit-code write and the orphan check atomically under one lock acquisition is what guarantees that exactly one of two paths reaps `p`: either a parent already blocked in `proc_wait`/`sys_waitpid` (if `p` was not an orphan when it exited), or `proc_exit` reaping itself immediately afterwards (if it was).
3. If `p` was an orphan, `proc_exit` calls `proc_destroy(p)` on itself right after signaling the wait condition variable, since no parent will ever call `waitpid` on it. Otherwise, `p`'s `struct proc` is left alive as a zombie — still present in `process_table`, holding its exit code — until the parent eventually calls `waitpid` and reaps it via `proc_destroy`.

This design means a `struct proc` is destroyed by exactly one of three call sites — `sys_waitpid` (normal parent-initiated reap), `proc_remove_all_children` (parent exits after an already-exited child), or `proc_exit` (a process that is itself an orphan at exit time) — and never by more than one, which is what the atomic exited-flag/parent-pointer update in step 2 is protecting against.

### Wait/Exit Synchronization

Blocking wait semantics are implemented with a per-process lock/condition-variable pair, `p_waitlock`/`p_waitcv`, rather than a global lock, so waiting on one process never blocks unrelated `fork`/`waitpid`/`_exit` activity on others.

- `proc_wait(proc)` acquires `p_waitlock` and loops on `cv_wait(p_waitcv, p_waitlock)` while `!p_exited`, returning `p_exitcode` once the condition is signaled. It is used both by `sys_waitpid` (for a live parent) and internally by `proc_remove_all_children` (to drain an already-exited child before destroying it).
- `proc_exit` acquires `p_waitlock`, sets `p_exited`/`p_exitcode` under `p_lock`, and calls `cv_signal(p_waitcv, p_waitlock)` before releasing `p_waitlock`, waking at most one waiter (a process only ever has one parent that can be blocked in `proc_wait` on it).

### Kernel Menu: Argument Passing and Process Waiting

The kernel menu (`src/kern/main/menu.c`) was modified in two related aspects: the passing of command-line arguments from the menu to the newly created process, and the synchronization of the menu with process termination, so that the menu prompt returns only after the started process has exited.

#### Argument Passing

In the base system, `cmd_progthread` only forwarded the program name to `runprogram()`, printing a warning if extra arguments were given. The menu now supports full argument passing:

1. `cmd_dispatch` tokenizes the typed command line into an `args[]` array (e.g. `p /testbin/argtest foo bar`), and `common_prog` forwards it to the new thread.
2. `cmd_progthread` passes the whole `args` array and the argument count to `runprogram(args, nargs)`, whose signature was changed accordingly (see `src/kern/syscall/runprogram.c`).
3. `runprogram` copies the argument strings and the `argv` vector onto the new user stack via `initstack()` (`src/kern/syscall/initstack.c`), then enters user mode with `enter_new_process(argc, argv, ...)`, so the program receives a standard `argc`/`argv` pair.

#### Waiting for Process Termination

In the base system, `common_prog` returned to the menu immediately after `thread_fork`, without waiting for the subprogram. Now, after forking, `common_prog`:

1. Saves the child PID and blocks in `proc_wait(proc)` until the child process exits (via `_exit`).
2. Destroys the reaped process with `proc_destroy`, then returns to the menu prompt.

Diagnostic PID information is printed only when `OPT_PROCDEBUG` is enabled.

If `runprogram` fails inside the child thread (e.g. the executable does not exist), `cmd_progthread` prints the error and calls `sys__exit(1)`, so the parent waiting in `proc_wait` always wakes up and the menu never hangs.

Blocking in `proc_wait` also removes the race condition noted in the original base-system comments: the menu loop no longer returns to the prompt (and therefore does not reuse its input buffer, which backs the `args` array) while the subprogram's thread is still reading it.

### The File Descriptor Table

File descriptor state is split across two structures, `struct open_file` and `struct fd_table` (`src/kern/filetable/filetable.c`), so that descriptors created by `dup2` or inherited through `fork` can correctly share the same underlying file rather than each holding an independent copy.

- `struct open_file` holds everything tied to a single opened file — the `vnode`, the current offset, the open flags, a reference count, and a lock protecting the offset — and lives in a fixed-size, system-wide table (`system_table`, sized at `10 * OPEN_MAX`), shared by every process.
- `struct fd_table` is per-process: an array of `OPEN_MAX` pointers into `system_table`, plus its own lock. It is declared as an opaque type in `filetable.h`; `struct proc` only ever holds a pointer to it (`p_fdtable`), never its contents, so process-management code cannot depend on file-table internals.

`fdtable_create_standard` builds the initial table for a new process, opening `con:` three times so that the first three calls to `fdtable_open` land on `STDIN_FILENO`, `STDOUT_FILENO`, and `STDERR_FILENO` in order. `dup2` (`fdtable_dup2`) and `fork` (`fdtable_clone`) do not duplicate the underlying `open_file` — they add another pointer to it and increment its reference count, so a `write` through one descriptor is immediately visible through any descriptor that shares it.

#### Locking Model

Three locks are involved:

| Lock | Scope | Protects |
| --- | --- | --- |
| `system_table_lock` | Global, one instance | Slot allocation in `system_table` and every `open_file`'s reference count |
| `ft_lock` | One per `fd_table` | The `ft_entries` array of a single process |
| `of_lock` | One per `open_file` | The offset, during `read`/`write`/`lseek` |

`system_table_lock` is always acquired before `ft_lock` when both are needed together (`fdtable_lookup_pinned`, `fdtable_dup2`, `fdtable_clone`), which avoids deadlock between them. `of_lock` is never held nested with either of the other two: every caller releases `ft_lock`/`system_table_lock` before acquiring `of_lock`, so `of_lock` is always taken on its own.

`of_lock` being per-file, rather than per-process, is what lets two different descriptors of the same process be read or written concurrently; two operations on the *same* shared descriptor (after `dup2`/`fork`) are still serialized, since they contend for the same `of_lock`.

#### Reference Counting

An `open_file`'s reference count is not only "how many descriptors point to this file" — it also counts operations currently in progress on it. `fdtable_lookup_pinned` fetches the `open_file` for a descriptor and increments its count in the same critical section, before releasing any lock; `system_table_release` decrements it back down, freeing the `open_file` (and closing its vnode) only once the count reaches zero. `fdtable_read` and `fdtable_write` bracket their work between these two calls. `fdtable_lseek` does not: it fetches the `open_file*` under `ft_lock` (released immediately after) and then operates under `of_lock` alone, without pinning the reference count for the duration.

This matters because a descriptor's entry alone does not stop a *different* process — one sharing the same `open_file` through `dup2`/`fork` — from closing its own copy while the first is still mid-`read`/`write`. Without the temporary increment, that close could bring the count to zero and free the `open_file` (destroying its lock) while the other side was still about to acquire it, corrupting kernel memory. Pinning the count for the duration of the operation guarantees the object outlives every operation that is actively using it, regardless of what any other process does with its own descriptor in the meantime.

## Tests

The following tests are user-space black-box integration tests that validate observable syscall behavior and error paths.

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

### `zombietest`

`zombietest` verifies zombie and orphan reaping semantics in three scenarios: a child that exits before the parent calls `waitpid` (zombie case), a child that exits before a parent that never waits, and a parent that exits before its child. It checks that `waitpid` still returns the correct PID and exit status for the zombie case, and that orphaned children are eventually reaped by the kernel without leaving stale process state.

## References

- [OS/161 Manual](https://people.ece.ubc.ca/os161/man/)
- [Linux Man Pages](https://man7.org/linux/man-pages/index.html)