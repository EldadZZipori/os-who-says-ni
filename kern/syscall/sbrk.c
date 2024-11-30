#include <addrspace.h>
#include <vm.h>
#include <current.h>
#include <proc.h>
#include <kern/errno.h>
#include <synch.h>
#include <types.h>
#include <freelist.h>

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

    /* We only allow page allinged sizes */
    if (amount % PAGE_SIZE != 0)
    {
        return -1; // Some issue here.
    }

    lock_acquire(as->heap_lk);

    int vpn1 = TLPT_MASK(as->user_heap_end);
    int vpn2 = LLPT_MASK(as->user_heap_end);

    if (amount < 0)
    {
        // Deallocate the physical memory here   

    }

    if (amount > 0)
    {
        // Allocate memory here
        /*
         * our as_create allocates one page for the top level page table itself.
         */

        vaddr_t va = alloc_kpages(amount/PAGE_SIZE);
        paddr_t pa = KSEG0_VADDR_TO_PADDR(va);          // Physical address of the new block we created.   

        vaddr_t* ll_pagetable_va;
        if (*(as->ptbase + vpn1) == 0)  // This means the low level page table for this top level page entery was not created yet
        {
            ll_pagetable_va = alloc_kpages(1); // allocate a single page for a low lever page table
            as_zero_region(ll_pagetable_va, 1); // zero all entries in the new low level page table.

            *(as->ptbase + vpn1) = (ll_pagetable_va) + 0b1; // Update the count of this low level page table to be 1
        } 
        else
        {
            ll_pagetable_va = TLPT_ENTRY_TO_VADDR(*(as->ptbase + vpn1));
            *(as->ptbase + vpn1) += 0b1;
        }

        (*(ll_pagetable_va + vpn2)) = pa; // this will be page aligned

        
        
    }

    as->user_heap_end += amount; // Change the heap break 


    lock_release(as->heap_lk);
    return 0;

}