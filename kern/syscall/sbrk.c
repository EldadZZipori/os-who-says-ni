#include <addrspace.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <synch.h>

int
sbrk (int amount, int* retval)
{
    struct addrspace* as = curproc->p_addrspace;

    if (as->user_heap_end + amount >= MIPS_KUSEG_END)
    {
        // problem cannot expand or retract the heap anymore
        return ENOMEM;
    }
    else if (as->user_heap_start + amount < MIPS_KUSEG)
    {
        return EINVAL;
    }

    if (amount == 0)
    {
        *retval = as->user_heap_end;
        return 0;
    }

    lock_acquire(as->heap_lk);

    if (amount < 0)
    {
        // Deallocate the physical memory here   


    }

    if (amount > 0)
    {
        // Allocate memory here
    }

    as->user_heap_end += amount; // Change the heap break 

    lock_release(as->heap_lk);
    return 0;

}