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

    curproc->exit_status = exitcode;

    /*
     * When exisiting, first cleanup all children who already exited 
     * but are waiting on a parent (us) to collect their status
     */
    for (int i = 0; i < curproc->children_size; i++)
    {
        /* 
         * When the child is a zombie destroy it.
         * destroy will also remove itself from the proc_table
         */
        if (curproc->children[i]->state == ZOMBIE)
        {
            proc_destroy(curproc->children[i]);
        }
        /*
         * When it is still running, make it aware that it is an orphan
         */
        else
        {
            curproc->children[i]->parent = NULL;
        }
    }

    // if we are an orphan we can just destroy ourself cause no one cares for us
    if (curproc->parent == NULL) 
    {
        proc_destroy(curproc); // Scary stuff will probabilty cause a problem
        return;
    }

    curproc->state = ZOMBIE; // if we are a zombie our parent will take care of killing us
    lock_acquire(curproc->children_lk);
    cv_signal(curproc->waiting_on_me,curproc->children_lk);
    lock_release(curproc->children_lk);


}