# Contributors

## Luca Ferrone
- Process structure with PID, parent PID, exit status, and wait/exit synchronization
- PID allocator: allocation, lookup, and reclamation
- `getpid`, `fork`, `waitpid`, `_exit`
- `kill_curthread` and `enter_forked_process`

## Matteo Francesco Castigliego
- Open-file table and file descriptor management
- `open`, `read`, `write`, `lseek`, `close`, `dup2`, `chdir`, `getcwd`

## Gabriele Ferrero
- `execv`
- Standard file descriptors initialization (`stdin`, `stdout`, `stderr` on `con:`) on process creation
- Syscall dispatcher (`syscall.c`)
- Kernel menu: passing command-line arguments to started programs and waiting for their termination
- Individual syscall testing and end-to-end integration testing via `bin/sh`
- EMUFS path tracking and `emufs_namefile` support