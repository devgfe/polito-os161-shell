/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <spl.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include "opt-shell.h"
#include "opt-procdebug.h"

#if OPT_SHELL
#include <filetable.h>
#include <kern/wait.h>
#include "synch.h"
#include "kern/errno.h"

#define PID_MAX 32767

/* Assign a unique process identifier (PID) to the kernel process. */
static void proc_init_kernel_pid(struct proc *proc);

/* Assign a unique process identifier (PID) to the given process. */
static int proc_assign_pid(struct proc *proc);

/* Returns an available PID, reusing a free one if the PID range has been exhausted. */
static int find_valid_pid(void);

static struct proc *process_table[PID_MAX + 1];
static struct lock *pid_lock; // lock for pid assignment
static pid_t next_pid;
#endif

/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
	struct proc *proc;

	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
		return NULL;
	}

	proc->p_numthreads = 0;
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

#if OPT_SHELL
	/* PID management state */
	proc->p_pid = NO_PARENT;
	proc->p_parent = NO_PARENT;
	proc->p_children = NULL;

	/* Wait/exit state */
	proc->p_exited = false;
	proc->p_exitcode = 0;
	proc->p_waitlock = NULL;
	proc->p_waitcv = NULL;

	/* FD table state */
	proc->p_fdtable = NULL;

	/* PID assignment */
	if (kproc != NULL) {
		int err = proc_assign_pid(proc);
		if (err) {
			proc_destroy(proc);
			return NULL;
		}
	}
	else {
		proc_init_kernel_pid(proc); // kernel process initialization
	}

	/* Wait/exit synchronization */
	proc->p_waitlock = lock_create("waitlock");
	if (proc->p_waitlock == NULL) {
		proc_destroy(proc);
		return NULL;
	}

	proc->p_waitcv = cv_create("waitcv");
	if (proc->p_waitcv == NULL) {
		proc_destroy(proc);
		return NULL;
	}
#endif

	return proc;
}

/*
 * Destroy a proc structure.
 *
 * Note: nothing currently calls this. Your wait/exit code will
 * probably want to do so.
 */
void
proc_destroy(struct proc *proc)
{
	/*
	 * You probably want to destroy and null out much of the
	 * process (particularly the address space) at exit time if
	 * your wait/exit design calls for the process structure to
	 * hang around beyond process exit. Some wait/exit designs
	 * do, some don't.
	 */

	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}

	/* VM fields */
	if (proc->p_addrspace) {
		/*
		 * If p is the current process, remove it safely from
		 * p_addrspace before destroying it. This makes sure
		 * we don't try to activate the address space while
		 * it's being destroyed.
		 *
		 * Also explicitly deactivate, because setting the
		 * address space to NULL won't necessarily do that.
		 *
		 * (When the address space is NULL, it means the
		 * process is kernel-only; in that case it is normally
		 * ok if the MMU and MMU- related data structures
		 * still refer to the address space of the last
		 * process that had one. Then you save work if that
		 * process is the next one to run, which isn't
		 * uncommon. However, here we're going to destroy the
		 * address space, so we need to make sure that nothing
		 * in the VM system still refers to it.)
		 *
		 * The call to as_deactivate() must come after we
		 * clear the address space, or a timer interrupt might
		 * reactivate the old address space again behind our
		 * back.
		 *
		 * If p is not the current process, still remove it
		 * from p_addrspace before destroying it as a
		 * precaution. Note that if p is not the current
		 * process, in order to be here p must either have
		 * never run (e.g. cleaning up after fork failed) or
		 * have finished running and exited. It is quite
		 * incorrect to destroy the proc structure of some
		 * random other process while it's still running...
		 */
		struct addrspace *as;

		if (proc == curproc) {
			as = proc_setas(NULL);
			as_deactivate();
		}
		else {
			as = proc->p_addrspace;
			proc->p_addrspace = NULL;
		}
		as_destroy(as);
	}

	KASSERT(proc->p_numthreads == 0);
	spinlock_cleanup(&proc->p_lock);

	kfree(proc->p_name);

#if OPT_SHELL
	/* Wait/exit state cleanup */
	if (proc->p_waitcv != NULL) {
		cv_destroy(proc->p_waitcv);
		proc->p_waitcv = NULL;
	}
	if (proc->p_waitlock != NULL) {
		lock_destroy(proc->p_waitlock);
		proc->p_waitlock = NULL;
	}

	/* FD table cleanup */
	if (proc->p_fdtable != NULL)
	{
		fdtable_destroy(proc->p_fdtable);
		proc->p_fdtable = NULL;
	}

	/* PID release */
	if (proc->p_pid >= 0 && proc->p_pid <= PID_MAX) {
		if (pid_lock != NULL) {
			pid_release(proc->p_pid);
		}
		else if (proc->p_pid == 0) {
			/* Early bootstrap fallback: pid_lock not created yet. */
			process_table[0] = NULL;
		}
	}
#endif

	kfree(proc);
}

/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void)
{
	kproc = proc_create("[kernel]");
	if (kproc == NULL) {
		panic("proc_create for kproc failed\n");
	}

#if OPT_SHELL
	/* PID allocator lock initialization. */
	pid_lock = lock_create("pid_lock");
	if (pid_lock == NULL)
	{
		panic("lock_create for pid_lock failed\n");
	}
#endif

}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *newproc;

	newproc = proc_create(name);
	if (newproc == NULL) {
		return NULL;
	}

	/* VM fields */

	newproc->p_addrspace = NULL;

	/* VFS fields */

	/*
	 * Lock the current process to copy its current directory.
	 * (We don't need to lock the new process, though, as we have
	 * the only reference to it.)
	 */
	spinlock_acquire(&curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		newproc->p_cwd = curproc->p_cwd;
	}
	spinlock_release(&curproc->p_lock);

#if OPT_SHELL
	/* PID management */
	/* Set parent from the calling process. */
	newproc->p_parent = curproc->p_pid;

	/* FD table management */
	/* Create the fd table for the new process and initialize the standard descriptors */
	newproc->p_fdtable = fdtable_create_standard();
	if (newproc->p_fdtable == NULL)
	{
		proc_destroy(newproc);
		return NULL;
	}

#if OPT_PROCDEBUG
	kprintf("Process %d created child process %d\n",
	        (int)newproc->p_parent, (int)newproc->p_pid);
#endif

#endif

	return newproc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int spl;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	proc->p_numthreads++;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = proc;
	splx(spl);

	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 *
 * Turn off interrupts on the local cpu while changing t_proc, in
 * case it's current, to protect against the as_activate call in
 * the timer interrupt context switch, and any other implicit uses
 * of "curproc".
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	int spl;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	KASSERT(proc->p_numthreads > 0);
	proc->p_numthreads--;
	spinlock_release(&proc->p_lock);

	spl = splhigh();
	t->t_proc = NULL;
	splx(spl);
}

/*
 * Fetch the address space of (the current) process.
 *
 * Caution: address spaces aren't refcounted. If you implement
 * multithreaded processes, make sure to set up a refcount scheme or
 * some other method to make this safe. Otherwise the returned address
 * space might disappear under you.
 */
struct addrspace *
proc_getas(void)
{
	struct addrspace *as;
	struct proc *proc = curproc;

	if (proc == NULL) {
		return NULL;
	}

	spinlock_acquire(&proc->p_lock);
	as = proc->p_addrspace;
	spinlock_release(&proc->p_lock);
	return as;
}

/*
 * Change the address space of (the current) process. Return the old
 * one for later restoration or disposal.
 */
struct addrspace *
proc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;

	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	spinlock_release(&proc->p_lock);
	return oldas;
}

#if OPT_SHELL

/* ========================= PID management ========================= */

void pid_release(pid_t pid)
{
	if (pid < 0 || pid > PID_MAX) {
		return;
	}

	lock_acquire(pid_lock);
	process_table[pid] = NULL;
    lock_release(pid_lock);
}

struct proc *proc_lookup(pid_t pid)
{
	if (pid < 0 || pid > PID_MAX) {
		return NULL;
	}

	struct proc *proc;

    lock_acquire(pid_lock);
    proc = process_table[pid];
    lock_release(pid_lock);

    return proc;
}

static int proc_assign_pid(struct proc *proc)
{
	lock_acquire(pid_lock);

	int pid = find_valid_pid();
	if (pid < 0)
	{
		lock_release(pid_lock);
		return ENPROC;
	}

	proc->p_pid = pid;
	process_table[pid] = proc;

	if (pid == next_pid && next_pid <= PID_MAX)
		next_pid++;

	lock_release(pid_lock);
	return 0;
}

static void proc_init_kernel_pid(struct proc *proc)
{
	proc->p_pid = 0;
	process_table[0] = proc;
	next_pid = 1;
}

static int find_valid_pid(void)
{
	if (next_pid <= PID_MAX)
		return next_pid;

	for (int i = 1; i <= PID_MAX; i++)
	{
		if (process_table[i] == NULL)
			return i;
	}

	return -1;
}

/* ===================== Wait/exit state management ===================== */

/*
 * Wait for the given process to exit.
 *
 * Blocks the calling thread on the process's wait condition variable
 * until the process sets p_exited (via sys__exit). Returns the
 * process's exit code.
 *
 * Note: the caller is responsible for destroying the process
 * structure afterwards with proc_destroy().
 */
int proc_wait(struct proc *proc)
{
	int exitcode;

	KASSERT(proc != NULL);

	lock_acquire(proc->p_waitlock);

	while (!proc->p_exited)
	{
		cv_wait(proc->p_waitcv, proc->p_waitlock);
	}

	exitcode = proc->p_exitcode;

	lock_release(proc->p_waitlock);

	return exitcode;
}

/*
 * Store the process exit code before terminating the current thread.
 *
 * The wait lock ensures that the exit code is written safely and can be read consistently by a waiting parent process.
 */

void proc_exit(int exitcode)
{
	struct proc *p = curproc;
	bool orphan;

	proc_remthread(curthread);

	proc_remove_all_children(p);

	/* Under p_lock, the exited flag and parent pointer are updated
	 * atomically, so exactly one of this exit path and the parent's
	 * proc_remove_all_children() reaps the process. */
	lock_acquire(p->p_waitlock);
	spinlock_acquire(&p->p_lock);
	p->p_exitcode = exitcode;
	p->p_exited = true;
	orphan = (p->p_parent == NO_PARENT);
	spinlock_release(&p->p_lock);
	cv_signal(p->p_waitcv, p->p_waitlock);
	lock_release(p->p_waitlock);

#if OPT_PROCDEBUG
	kprintf("Process %d terminated with status coded=%d, pure=%d\n",
	        (int)p->p_pid, exitcode, WEXITSTATUS(exitcode));
#endif

	if (orphan) {
		/*
		 * Parent is dead: nobody will ever wait for us, so we can
		 * destroy ourselves immediately.
		 */
		proc_destroy(p);
	}

	thread_exit();
	
	panic("thread_exit returned\n");
}

int proc_add_child(struct proc *parent, pid_t pid)
{
	struct proc_node *child;

	child = kmalloc(sizeof(*child));
	if (child == NULL) {
		return ENOMEM;
	}

	child->pid = pid;

	spinlock_acquire(&parent->p_lock);

	child->next = parent->p_children;
	parent->p_children = child;

	spinlock_release(&parent->p_lock);

	return 0;
}

int proc_remove_child(struct proc *parent, pid_t pid)
{
	struct proc_node *curr;
	struct proc_node *prev;

	spinlock_acquire(&parent->p_lock);

	prev = NULL;
	curr = parent->p_children;

	while (curr != NULL) {
		if (curr->pid == pid) {
			if (prev == NULL) {
				parent->p_children = curr->next;
			} else {
				prev->next = curr->next;
			}

			spinlock_release(&parent->p_lock);
			kfree(curr);

			return 0;
		}

		prev = curr;
		curr = curr->next;
	}

	spinlock_release(&parent->p_lock);

	return ESRCH;
}

void proc_remove_all_children(struct proc *parent){
	struct proc_node *curr;
	struct proc_node *next;
	struct proc *child;
	bool reap;
#if OPT_PROCDEBUG
	pid_t child_pid;
	pid_t child_parent_pid;
#endif

	spinlock_acquire(&parent->p_lock);

	curr = parent->p_children;
	parent->p_children = NULL;

	spinlock_release(&parent->p_lock);

	while (curr != NULL) {
		next = curr->next;

		child = proc_lookup(curr->pid);

		reap = false;

		if (child != NULL) {
			spinlock_acquire(&child->p_lock);

#if OPT_PROCDEBUG
			child_pid = child->p_pid;
			child_parent_pid = child->p_parent;
#endif

			child->p_parent = NO_PARENT;

			/*
			 * If the child already exited, reap it immediately.
			 */
			reap = child->p_exited;

			spinlock_release(&child->p_lock);
		}

		if (reap) {
			(void)proc_wait(child);
			proc_destroy(child);

#if OPT_PROCDEBUG
			kprintf("Process %d collected orphan process %d during parent exit (process parent pid=%d)\n",
					(int)parent->p_pid, (int)child_pid, (int)child_parent_pid);
#endif
		}

		kfree(curr);
		curr = next;
	}
}

#endif