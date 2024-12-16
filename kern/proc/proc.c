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
#include <synch.h>
#include <proc.h>
#include <current.h>
#include <proctable.h>
#include <filetable.h>
#include <abstractfile.h>
#include <addrspace.h>
#include <vnode.h>
#include <kern/errno.h>

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

	threadarray_init(&proc->p_threads);
	spinlock_init(&proc->p_lock);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;

	/* 
	 * Added for Assignment 4
	 * When a process is created it should always have three file descriptors for it
	 */
	proc->fdtable_num_entries = 3;

	/*
	 * If the file table already exists 
	 * i.e. the vfs is initilized too
	 */
	proc->fdtable_lk = lock_create("fd lk");
	
	for (int i = 0; i< MAX_CHILDREN_PER_PERSON; i++)
	{
		proc->children[i] = NULL;
	}

	/*
	 * The file descriptor table will only be bootstraped after 
	 * the kernel. So we cannot and should not increase the ref_count
	 * before it is created.
	 */
	// this is not needed as the only was to create a process is to fork it
	if (kfile_table != NULL)
	{
		// more for next assignment
		//lock_acquire(kfile_table->files_lk[0]);
		kfile_table->files[0]->ref_count++;
		//lock_release(kfile_table->files_lk[0]);

		//lock_acquire(kfile_table->files_lk[1]);
		kfile_table->files[1]->ref_count++;
		//lock_release(kfile_table->files_lk[1]);

		//lock_acquire(kfile_table->files_lk[2]);
		kfile_table->files[2]->ref_count++;
		//lock_release(kfile_table->files_lk[2]);
		
	}

	/*
	 * IMPORTANT NOTE - the main open file descriptor table 
	 * is in-charge of creating these files in reality.
	 */
	proc->fdtable[0] = 0;
    proc->fdtable[1] = 1;
    proc->fdtable[2] = 2;

	/*
	 * Make sure that all file descripors indicate that they are empty 
	 */
	for (int i = 3; i < __OPEN_MAX; i++)
	{
		proc->fdtable[i] = FDTABLE_EMPTY;
	}

	if (kfile_table != NULL) // Will only be false for the kernel
	{
		/* For Assignment 5 - add pid functionality */
		lock_acquire(kproc_table->pid_lk);

		/*
		 * Find an available pid in the process table, this should have probably
		 * been done before this is called as well, to give a better indication of why the 
		 * process cannot be created.
		 * If no pid is available will return null.
		 */
		int pid = pt_find_avail_pid(); 
		if (pid == MAX_PID_REACHED)
		{
			lock_release(kproc_table->pid_lk);
			proc_destroy(proc);
			return NULL;
		}
		
		/*
		 * Adds the newly created process to the process table
		 * This is done here so *EVERY* process is in the table
		 * not just forked ones
		 */
		if (pt_add_proc(proc, pid)) 
		{
			lock_release(kproc_table->pid_lk);
			proc_destroy(proc);
		    return NULL;
		}
		lock_release(kproc_table->pid_lk);

		proc->waiting_on_me = cv_create("Proc conditional variable");
		if (proc->waiting_on_me == NULL)
		{
			lock_release(kproc_table->pid_lk);
			proc_destroy(proc);
			return NULL;
		}

		proc->parent = curproc;

		lock_acquire(curproc->children_lk);
		curproc->children[curproc->children_size] = proc;
		curproc->children_size++;
		lock_release(curproc->children_lk);
	}
	else	// If this is the kernel
	{
		proc->parent = NULL;
	}

	proc->state = CREATED;
	proc->children_lk = lock_create("children lk");
	if (proc->children_lk == NULL)
	{
		lock_release(kproc_table->pid_lk);
		proc_destroy(proc);
		return NULL;
	}
	proc->children_size = 0;

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
	// ASSIGNMENT 5 remember to check wher kproc get destroed. Remeber to remove all open files in thei process

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



	threadarray_cleanup(&proc->p_threads);
	spinlock_cleanup(&proc->p_lock);


	/* Assignment 4 - File related clearnups */

	// Make sure to close all references to the files once the process is done.
	for (unsigned int i = 0; i < proc->fdtable_num_entries; i++)
	{
		__close(proc ,proc->fdtable[i]);
	}
	lock_destroy(proc->fdtable_lk);


	/* Assignment 5 */
	// TODO - must remove itself from process table!!!
	lock_acquire(kproc_table->pid_lk);
	pt_remove_proc(proc->p_pid);
	lock_release(kproc_table->pid_lk);


	// tell all our children we are dead
	// for (int i = 0; i < proc->children_size; i++)
	// {
	// 	proc->children[i]->parent = NULL;
	// }
	/* 
	 * Check we are not called because one of these failed
	 */
	if (proc->children_lk != NULL)
	{
		/*
		 * Only exit (and proc_create) calls this,
		 * we are either destroying because these are null or when exiting, 
		 * meaning we *MUST* hold the children_lk
		 */
		lock_release(proc->children_lk);
		lock_destroy(proc->children_lk);
	}

	if (proc->waiting_on_me != NULL)
	{
		cv_destroy(proc->waiting_on_me);
	}

	kfree(proc->p_name);
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

	kproc->state = RUNNING;
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
	int result;
	int spl;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_lock);
	result = threadarray_add(&proc->p_threads, t, NULL);
	spinlock_release(&proc->p_lock);
	if (result) {
		return result;
	}
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
	unsigned i, num;
	int spl;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_lock);
	/* ugh: find the thread in the array */
	num = threadarray_num(&proc->p_threads);
	for (i=0; i<num; i++) {
		if (threadarray_get(&proc->p_threads, i) == t) {
			threadarray_remove(&proc->p_threads, i);
			spinlock_release(&proc->p_lock);
			spl = splhigh();
			t->t_proc = NULL;
			splx(spl);
			return;
		}
	}
	/* Did not find it. */
	spinlock_release(&proc->p_lock);
	panic("Thread (%p) has escaped from its process (%p)\n", t, proc);
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

int
__check_fd(int fd)
{
	if (fd < 0 || fd >= OPEN_MAX) return EBADF;

	return 0;
}

int
__copy_fd_table(struct proc* from, struct proc* to)
{
	if (to == NULL || from == NULL)
	{
		return EINVAL;
	}
	// First three are created by proc_create and are the same
	int fd = 0;
	for (unsigned int i=3; i < from->fdtable_num_entries; i++)
	{
		fd = from->fdtable[i];
		to->fdtable[i] = fd;
		//lock_acquire(kfile_table->files_lk[fd]);
		lock_acquire(kfile_table->location_lk);
		kfile_table->files[fd]->ref_count++; // Remeber to add the reference
		lock_release(kfile_table->location_lk);
		//lock_release(kfile_table->files_lk[fd]);

		VOP_INCREF(kfile_table->files[fd]->vn); // To the vnode too~!!!!
	}

	return 0;
}