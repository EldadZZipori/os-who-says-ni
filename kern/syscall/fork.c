#include <types.h>
#include <kern/errno.h>
#include <synch.h>
#include <proc.h>
#include <mips/trapframe.h>
#include <addrspace.h>
#include <proctable.h>
#include <filetable.h>
#include <syscall.h>
#include <copyinout.h>
#include <current.h>



/**
 * Function prototypes
 */
/**
 * @brief local function to serve as entrty point for the new forked process
 */
static void child_return(void* data1, unsigned long data2);


/** 
 * @brief system-level function for forking a process 
 *
 * @param tf user-space pointer to the current process' trapframe upon exception entry
 * 
 * @return 0 on success, error code on failure
 */
int
sys_fork(struct trapframe *tf, int *retval)
{
    int err;
    struct proc *new_proc;

    // Return value only changes if no error happened
    *retval = -1; 

    // Check that the process table for the system is not full
	if (kproc_table->process_counter == __PID_MAX)
	{
		return EMPROC;
	}

    // check user doesn't already have too many processes
    // if already too many (for this user), return EMPROC, *not* ENPROC
    // TODO Assignment 5: Double check - there seems to be no user-process limit

    // Create a new process. NOTE: most is done in proc_create which cannot be accessed
    new_proc = proc_create_runprogram("forked process");
    if (new_proc == NULL) { 
        return ENOMEM; // ran out of space when kmalloc-ing proc
    }

    // 1. copy address space
    // TODO Assignment 5: Acquire locks for both processes?
    err = as_copy(curproc->p_addrspace, &new_proc->p_addrspace);
    if (err) {
        lock_acquire(new_proc->children_lk);
        proc_destroy(new_proc);
        return ENOMEM;
    }

    // 2. copy file table 
    err = __copy_fd_table(curproc, new_proc);
    if (err) {
        lock_acquire(new_proc->children_lk);
        proc_destroy(new_proc);
        return ENOMEM;
    }

    // 4. copy kernel thread 
    struct trapframe tf_copy = *tf;

    err = thread_fork("forked thread", 
                new_proc,
                child_return,
                &tf_copy, // malloc'd
                0);

    if (err) {
        lock_acquire(kproc_table->pid_lk);
        pt_remove_proc(new_proc->my_pid);
        lock_release(kproc_table->pid_lk);
        
        lock_acquire(new_proc->children_lk);
        proc_destroy(new_proc);
        return err;
    }

    /*
     * Why this works?
     * The child cannot destroy itself untill the parent destroies itself.
     * As long as a process has a parent it will only become a zombie.
     * This assures that the child grabed the trapframe, which is on the parent's stack
     * before exiting the parent's function call (which will otherwise override the stack).
     * WARNING: this might look like it should have a cv, but that will actually cause many race conditions
     * and be harder to handle. 
     * NOTE: state is a volitaile variable
     */
    while(new_proc->state == CREATED);

    // now that thread_fork has been called, only the parent thread executes the following
    // return child pid (only parent runs this)
    *retval = new_proc->my_pid;
    return 0;
}

/**
 * @brief a thread_fork() compatible function to run a new thread in the child and return
 * Should only actually activate things in the child here
 */
static
void
child_return(void* data1, unsigned long data2)
{
    (void) data2;
    struct trapframe child_tf; 

    child_tf = *((struct trapframe*)data1); // copy parent trapframe to user stack

    proc_setas(curproc->p_addrspace); // Set the new address space for the child process
    as_activate();  // Activates the new address space for the process

    curproc->state = RUNNING;
    enter_forked_process(&child_tf);
}
