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
    spinlock_acquire(&curproc->p_lock);
    struct addrspace* as = curproc->p_addrspace;

    lock_acquire(as->heap_lk);

    int abs_amount;
    bool is_negative;
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

    if (as->user_heap_end + amount >= as->user_stackbase)
    {
        // problem cannot expand or retract the heap anymore
        lock_release(as->heap_lk);
        spinlock_release(&curproc->p_lock);
        return ENOMEM;
    }
    else if (as->user_heap_end + amount < as->user_heap_start)
    {
        lock_release(as->heap_lk);
        spinlock_release(&curproc->p_lock);
        return EINVAL;
    }

    if (amount == 0)
    {
        *retval = as->user_heap_end;
        lock_release(as->heap_lk);
        spinlock_release(&curproc->p_lock);
        return 0;
    }

    /* We only allow page allinged sizes */
    if (abs_amount % PAGE_SIZE != 0)
    {
        lock_release(as->heap_lk);
        spinlock_release(&curproc->p_lock);
        return -1; // Some issue here.
    }

    int npages = abs_amount / PAGE_SIZE;
    int result;

    *retval = as->user_heap_end;

    if (is_negative)
    {
        // Deallocate the physical memory here   
        free_heap_upages(as, npages);

    }
    else 
    {
        result = alloc_heap_upages(as, npages);
        if (result)
        {
            lock_release(as->heap_lk);
            spinlock_release(&curproc->p_lock);
            return result;
        }
    }

    lock_release(as->heap_lk);
    spinlock_release(&curproc->p_lock);
    return 0;

}