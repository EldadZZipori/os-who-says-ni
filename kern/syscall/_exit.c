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


void
sys__exit(int exitcode)
{
    struct proc* calling_proc;

    lock_acquire(curproc->children_lk);
    calling_proc = curproc;
    calling_proc->exit_status = exitcode;
    
    /*
     * When exisiting, first cleanup all children who already exited 
     * but are waiting on a parent (us) to collect their status
     */
    for (int i = 0; i < calling_proc->children_size; i++)
    {
        /* 
         * When the child is a zombie destroy it.
         * destroy will also remove itself from the proc_table
         */
        if (calling_proc->children[i]->state == ZOMBIE)
        {
            lock_acquire(calling_proc->children[i]->children_lk);
            proc_destroy(calling_proc->children[i]);
        }
        /*
         * When it is still running, make it aware that it is an orphan
         */
        else
        {
            calling_proc->children[i]->parent = NULL;
        }
    }

    // if we are an orphan we can just destroy ourself cause no one cares for us
    if (calling_proc->parent == NULL) 
    {
        proc_remthread(curthread);
        proc_destroy(calling_proc); // Scary stuff will probabilty cause a problem
        thread_exit();
    }

    calling_proc->state = ZOMBIE; // if we are a zombie our parent will take care of killing us
    cv_signal(calling_proc->waiting_on_me,calling_proc->children_lk);
    
    proc_remthread(curthread);
    lock_release(calling_proc->children_lk);
    thread_exit();

}