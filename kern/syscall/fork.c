#include <types.h>
#include <errno.h>
#include <proc.h>

int
sys_fork(struct trapframe* tf)
{
    int err;

    // lock the proctable 

    // check user doesn't already have too many processes
    // if already too many (for this user), return EMPROC, *not* ENPROC

    // check pid available - system-wide process count
    // if not, return ENPROC, *not* EMPROC

    // create proc
    struct proc *new = proc_create_runprogram("");
    if (new == NULL) { 
        // proc_create failed, handle this error

        return ENOMEM; // ran out of space when kmalloc-ing proc
    }

    // add to proctable

    // 1. copy address space

    // 2. copy file table 

    // 3. copy architectural state - in tf

    // 4. copy kernel thread 
    // entrypoint: enter_forked_process
    // arg: tf

    // now that thread_fork has been called, only the parent thread executes the following

    // return child pid (only parent runs this)
    *retval = new->pid;
    return 0;
}