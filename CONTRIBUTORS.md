# Contributors

## Luca Ferrone
- Extended the process structure to support PID, parent PID, exit status, and synchronization primitives
- PID allocator: allocation, lookup, and reclamation
- `getpid`, `fork`, `waitpid`, `_exit`
- `kill_curthread` and `enter_forked_process`

## Matteo Francesco Castigliego
- Per-process open-file table (maps file descriptors to open-file description entries)
- System-wide open-file table (vnode, offset, flags, reference count, lock)
- `open`, `read`, `write`, `lseek`, `close`, `dup2`, `chdir`, `getcwd`

## Gabriele Ferrero
- `execv`
- Standard file descriptors initialization (`stdin`, `stdout`, `stderr` on `con:`) on process creation
- Syscall dispatcher (`syscall.c`)
- Individual syscall testing and end-to-end integration testing via `bin/sh`