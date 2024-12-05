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
        result = copyout(kstatus, status, sizeof(int));
    }
    
    kfree(kstatus);
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
            if (curproc->children[i]->p_pid == pid)
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
    if(status != NULL)
    {
        *status = child->exit_status;
    }
    lock_release(child->children_lk);
    if (curproc->parent == NULL) // if we are the kernel, clean our children when they return
    {
        for (int i = 0; i < curproc->children_size; i++)
        {
            if (curproc->children[i] != NULL)
            {
                lock_acquire(curproc->children[i]->children_lk);
                proc_destroy(curproc->children[i]);
                curproc->children[i] = NULL;
            }

        }
        
    }
    

    return 0;
}