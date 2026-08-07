#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <current.h>
#include "opt-shell.h"

/*
 * simple proc management system calls
 */
void sys__exit(int status)
{
  /* get address space of current process and destroy */
  struct addrspace *as = proc_getas();
  as_destroy(as);
  /* thread exits. proc data structure will be lost */
  thread_exit();

  panic("thread_exit returned (should not happen)\n");
  (void) status; // TODO: status handling
}


int sys_getpid(pid_t *retval){
  *retval = curproc->pid;
  return 0;
}