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
#include <vm.h>


int
sys_sbrk (int amount, int* retval)
{
    // lock the current process so no one can change the addressspace while we do this
    struct addrspace* as = curproc->p_addrspace;

    int abs_amount;
    bool is_negative;
    
    // determine if we need to allocate or deallocate space
    if (amount < 0)
    {
        abs_amount = -1 * amount;
        is_negative = true;
    }
    else
    {
        abs_amount = amount;
        is_negative = false;
    }

    // check that the user has not reached the stack ares
    if (as->user_heap_end + amount >= as->user_stackbase)
    {
        // problem cannot expand or retract the heap anymore
        return ENOMEM;
    }
    // or is trying to deallocate below where the heap starts
    else if (as->user_heap_end + amount < as->user_heap_start)
    {
        return EINVAL;
    }

    // when the amount is zero, just return the currect location of the heap end
    if (amount == 0)
    {
        *retval = as->user_heap_end;
        return 0;
    }

    /* We only allow page allinged sizes */
    if (abs_amount % PAGE_SIZE != 0)
    {
    
        return -1; // Some issue here.
    }

    int npages = abs_amount / PAGE_SIZE;
    int result;

    *retval = as->user_heap_end;

    // deallocate if if amount was negative
    if (is_negative)
    {   
        free_heap_upages(as, npages);
        return 0;

    }
    // allocate if possitive.
    else 
    {
        result = alloc_heap_upages(as, npages);
        if (result)
        {
            return result;
        }
    }

    return 0;

}