#include <types.h>
#include <errno.h>
#include <proc.h>
#include <addrspace.h>
#include <proctable.h>
#include <current.h>
#include <filetable.h>

int
sys_fork(struct trapframe* tf)
{
    int err;
    int pid;

    // lock the proctable 

    // check user doesn't already have too many processes
    // if already too many (for this user), return EMPROC, *not* ENPROC

    // check pid available - system-wide process count
    // if not, return ENPROC, *not* EMPROC

    // create proc
    struct proc *new_proc = proc_create_runprogram("");
    if (new_proc == NULL) { 
        // proc_create failed, handle this error

        return ENOMEM; // ran out of space when kmalloc-ing proc
    }

    // add to proctable
    pid = pt_add_proc(new_proc);

    // 1. copy address space
    as_copy(curproc, new_proc);
    // 2. copy file table 
    __copy_fd_table(curproc, new_proc);

    // 3. copy architectural state - in tf

    // 4. copy kernel thread 
    // entrypoint: enter_forked_process
    // arg: tf

    // now that thread_fork has been called, only the parent thread executes the following

    // return child pid (only parent runs this)
    *retval = pid;
    return 0;
}