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
sys_waitpid(int pid, userptr_t status, int options ,int* retval)
{
    int result;

    *retval = pid;
    int* kstatus = kmalloc(sizeof(int));


    result = __waitpid(pid,kstatus, options);
    if (result)
    {
        kfree(kstatus);
        return result;
    }

    if (status != NULL)
    {
        result = copyout(kstatus, status, sizeof(status));
    }
    
   

    return result;

    
}

int
__waitpid(int pid, int* status, int options)
{
    int result;
    struct proc* child;
    //int* status_i = (int*) &status;

    lock_acquire(kproc_table->pid_lk);

    if (options != 0)
    {
        lock_release(kproc_table->pid_lk);
        return EINVAL;
    }

    result = pt_check_pid(pid);

    if (result) 
    {
        lock_release(kproc_table->pid_lk);
        return result;
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
        else
        {
            break;
        }
    }
    lock_release(kproc_table->pid_lk);

    if(!result)
    {
        return ECHILD;
    }

    // if child already exited we can just return
    lock_acquire(child->children_lk);
    if (child->state != ZOMBIE)
    {
        cv_wait(child->waiting_on_me, child->children_lk);
    }

    *status = child->exit_status;

    lock_release(child->children_lk);

    return 0;
}