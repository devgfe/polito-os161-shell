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
#include <machine/trapframe.h>


#if OPT_SHELL
  #include <kern/errno.h>
  #include <synch.h>
  #include <kern/wait.h>

  void sys__exit(int exitcode)
  {
    struct proc *p = curproc;

    lock_acquire(p->p_waitlock);

    p->p_exitcode = _MKWAIT_EXIT(exitcode);
    p->p_exited = true;

    cv_broadcast(p->p_waitcv, p->p_waitlock);

    lock_release(p->p_waitlock);

    thread_exit();
    panic("thread_exit returned\n");
  }


  int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval)
  {
    /*
    * OS/161 currently supports only options == 0.
    * The parameter is kept for compatibility with the standard POSIX waitpid interface,
    * where additional flags (e.g., WNOHANG) may be supported in the future.
    */
    if (options != 0) {
        return EINVAL;
    }

    struct proc *child = proc_lookup(pid);

    if (child == NULL) {
        return ESRCH;
    }

    if (child->p_parent != curproc->p_pid) {
      return ECHILD;
    }

    lock_acquire(child->p_waitlock);

    while (!child->p_exited) {
        cv_wait(child->p_waitcv, child->p_waitlock);
    }

    int code = child->p_exitcode;

    lock_release(child->p_waitlock);

    int err = copyout(&code, status, sizeof(int));
    if (err) {
        return err;
    }


    proc_destroy(child);

    *retval = pid;
    return 0;
  }



  int sys_getpid(pid_t *retval){
    *retval = curproc->p_pid;
    return 0;
  }



  int sys_fork(struct trapframe *tf, pid_t *retval){
    int err;
    struct proc *child;
    struct addrspace *child_as;
    struct trapframe *child_tf;

    child_tf = kmalloc(sizeof(struct trapframe));
    if (child_tf == NULL) {
        return ENOMEM;
    }

    *child_tf = *tf;

    child = proc_create_runprogram(curproc->p_name);
    if (child == NULL) {
        kfree(child_tf);
        return ENOMEM;
    }
    // Miglioramento possibile, da discutere: implementare la copy on write
    err = as_copy(curproc->p_addrspace, &child_as);
    if (err) {
        proc_destroy(child);
        kfree(child_tf);
        return err;
    }

    child->p_addrspace = child_as;

    // TO-DO : fare la copia della file descriptor table

    child->p_parent = curproc->p_pid;

    err = thread_fork(
        curthread->t_name,
        child,
        enter_forked_process, 
        child_tf,
        0
    );

    if (err) {
        proc_destroy(child);
        kfree(child_tf);
        return err;
    }

    *retval = child->p_pid;
    return 0;
  }

#endif