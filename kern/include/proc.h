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

#ifndef _PROC_H_
#define _PROC_H_

/*
 * Definition of a process.
 *
 * Note: curproc is defined by <current.h>.
 */

#include <spinlock.h>
#include <thread.h> /* required for struct threadarray */

#include <limits.h>

/**	
 * Constants
 */
#define FDTABLE_EMPTY -1
#define MAX_CHILDREN_PER_PERSON 32

struct addrspace;
struct vnode;


typedef enum {
	CREATED,
    RUNNING,
	SLEEPING,
    ZOMBIE
} procstate_t;

/*
 * Process structure.
 */
struct proc {
	char *p_name;			/* Name of this process */
	struct spinlock p_lock;		/* Lock for this structure */
	struct threadarray p_threads;	/* Threads in this process */

	/* VM */
	struct addrspace *p_addrspace;	/* virtual address space */

	/* VFS */
	struct vnode *p_cwd;		/* current working directory */

	/* add more material here as needed */
	/* Added members for Assignment 4 */

	/* 
	 * Open File Descriptor table 
	 * maps a process's file decriptor (fd) to its actual index in the
	 * open file table of the whole system
	 * 
	 * Comes with a lock to avoid race conditions on fd during open/close/read/etc operations
	 */
	int fdtable[__OPEN_MAX];		
	struct lock* fdtable_lk;

	//struct lock** fdtable_index_lks; TBD if needed

	/* Amount of currently opened files for this proccess */	
	unsigned int fdtable_num_entries;

	int p_pid;

	/* Waitpid and exit added functionality */
	struct proc* parent;
	struct cv* waiting_on_me;
	struct proc* children[MAX_CHILDREN_PER_PERSON];

	int children_size;
	struct lock* children_lk;

	procstate_t state;
	volatile int exit_status;
};

/* This is the process structure for the kernel and for kernel-only threads. */
extern struct proc *kproc;

/* Call once during system startup to allocate data structures. */
void proc_bootstrap(void);

/* Create a fresh process for use by runprogram(). */
struct proc *proc_create_runprogram(const char *name);

/* Destroy a process. */
void proc_destroy(struct proc *proc);

/* Attach a thread to a process. Must not already have a process. */
int proc_addthread(struct proc *proc, struct thread *t);

/* Detach a thread from its process. */
void proc_remthread(struct thread *t);

/* Fetch the address space of the current process. */
struct addrspace *proc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *proc_setas(struct addrspace *);


/* Added functionality for Assignment 4 */
/**
 * @brief helper function to use to validate a valid file descriptor
 * 
 * @return returns 0 if file descriptor is valid, comply with errno
 */
int
__check_fd(int fd);

/* Added functionality for Assignment 5 */
int
__copy_fd_table(struct proc* from, struct proc* to);
#endif /* _PROC_H_ */
