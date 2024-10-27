#include <types.h>
#include <kern/errno.h>
#include <synch.h>
#include <proc.h>
#include <mips/trapframe.h>
#include <addrspace.h>
#include <proctable.h>
#include <current.h>
#include <filetable.h>
#include <syscall.h>
#include <copyinout.h>



/**
 * Function prototypes
 */
//static int copy_trapframe(struct trapframe* old_tf, struct trapframe* new_tf);
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
    pid_t pid;
    //char proc_name[20];
    struct proc *new_proc;

    // lock the proctable 
    //lock_acquire(kproc_table->pid_lk);

    *retval = -1; // only change if there is no error

    // check user doesn't already have too many processes
    // if already too many (for this user), return EMPROC, *not* ENPROC
    // TODO Assignment 5: Double check - there seems to be no user-process limit

    // create string called "process {pid}" as a stack var
    //snprintf(proc_name, 20, "process %d", pid);

    // debug msg
    //printf("Assigng child process name: %s\n", proc_name);

    // create proc
    new_proc = proc_create_runprogram("forked process");
    if (new_proc == NULL) { 
        //lock_release(kproc_table->pid_lk);
        return ENOMEM; // ran out of space when kmalloc-ing proc
    }

    pid = new_proc->my_pid;

    // 1. copy address space
    // TODO Assignment 5: Acquire p locks for both processes?
    err = as_copy(curproc->p_addrspace, &new_proc->p_addrspace);
    if (err) {
       //lock_release(kproc_table->pid_lk);
        proc_destroy(new_proc);
        return ENOMEM;
    }
    // 2. copy file table 
    // TODO Assignment 5: Does curproc need to be copyin'd???
    err = __copy_fd_table(curproc, new_proc);
    if (err) {
        //lock_release(kproc_table->pid_lk);
        proc_destroy(new_proc);
        return ENOMEM;
    }

    // add to proctable - moved to proc_create
    // err = pt_add_proc(new_proc, pid);
    // if (err) {
    //     lock_release(kproc_table->pid_lk);
    //     proc_destroy(new_proc);
    //     return ENPROC;
    // }

    // done with proctable
    //lock_release(kproc_table->pid_lk);

    // 4. copy kernel thread 
    // entrypoint: enter_forked_process(struct trapframe *tf)
    // arg: tf
    // to user this function we must have a function (void*, unsigned long)
    // TODO: change thread name to be unique 
    err = thread_fork("forked thread", 
                new_proc,
                child_return,
                tf,
                0);

    if (err) {
        pt_remove_proc(new_proc);
        proc_destroy(new_proc);
        return err;
    }

    // now that thread_fork has been called, only the parent thread executes the following
    // return child pid (only parent runs this)
    *retval = pid;
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

    enter_forked_process(&child_tf);
}
