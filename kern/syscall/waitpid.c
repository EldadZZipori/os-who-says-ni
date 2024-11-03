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
#include <kern/errno.h>
#include <copyinout.h>


int
sys__waitpid(int pid, int *status, int options ,int* retval)
{
    int result;
    struct proc* child;
    lock_acquire(kproc_table->pid_lk);

    if (options != 0)
    {
        return EINVAL;
    }

    result = pt_check_pid(pid);

    if (!result) 
    {
        lock_release(kproc_table->pid_lk);
        return ESRCH;
    }

    result = 0;
    for (int i = 0; i < MAX_CHILDREN_PER_PERSON; i++)
    {
        if (curproc->children[i] != NULL)
        {
            if (curproc->children[i]->my_pid == pid)
            {
                result = 1; // pid belongs to child 
                child = curproc->children[i];
            }
        }
    }

    if(!result)
    {
        lock_release(kproc_table->pid_lk);
        return ECHILD;
    }

    // if child already existed we can just return
    if (child->state != ZOMBIE)
    {
        lock_acquire(child->children_lk);
        cv_wait(child->waiting_on_me, child->children_lk);
        lock_release(child->children_lk);
    }

    *retval = pid;
    if (status != NULL)
    {
        *status = child->exit_status;
    } 
    return 0;

    lock_release(kproc_table->pid_lk);
}