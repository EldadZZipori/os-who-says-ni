#include <addrspace.h>
#include <vm.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <synch.h>
#include <types.h>
#include <memlist.h>

int
sbrk (int amount, int* retval)
{
    spinlock_acquire(&curproc->p_lock);
    struct addrspace* as = curproc->p_addrspace;

    lock_acquire(as->heap_lk);

    if (as->user_heap_end + amount >= as->user_stackbase)
    {
        // problem cannot expand or retract the heap anymore
        return ENOMEM;
    }
    else if (as->user_heap_end + amount < as->user_heap_end)
    {
        return EINVAL;
    }

    if (amount == 0)
    {
        *retval = as->user_heap_end;
        return 0;
    }

    /* We only allow page allinged sizes */
    if (amount % PAGE_SIZE != 0)
    {
        return -1; // Some issue here.
    }

    int npages = amount / PAGE_SIZE;
    int result;


    if (amount < 0)
    {
        // Deallocate the physical memory here   

    }

    if (amount > 0)
    {
        result = alloc_heap_upages(as, npages);
        if (result)
        {
            return result;
        }
    }

    lock_release(as->heap_lk);
    spinlock_release(&curproc->p_lock);
    return 0;

}