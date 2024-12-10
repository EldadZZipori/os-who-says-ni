/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <synch.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <vnode.h>
#include <bitmap.h>
#include <kern/swapspace.h>


int 
as_create_stack(struct addrspace* as)
{
	bool in_swap;
	vaddr_t stack_va = USERSPACETOP - (DUMBVMER_STACKPAGES * PAGE_SIZE);
	int result = alloc_upages(as, &stack_va, DUMBVMER_STACKPAGES, &in_swap, false,1,1,0); 
	if (result)
	{
		return result;
	}
	vaddr_t stacktop = stack_va;
	if (stacktop != USERSPACETOP) {
		return ENOMEM;
	}
	as->user_stackbase = USERSPACETOP - (DUMBVMER_STACKPAGES * PAGE_SIZE);
	if (in_swap)
	{
		for (int i = 0; i < DUMBVMER_STACKPAGES; i++)
		{
			int swap_idx = LLPTE_GET_SWAP_OFFSET(translate_vaddr_to_paddr(as, as->user_stackbase + (i * PAGE_SIZE)));
			zero_swap_page(swap_idx);
		}
		
	}
	else
	{
		as_zero_region(as->user_stackbase, DUMBVMER_STACKPAGES);
	}
	

	return 0;
}

void
as_zero_region(vaddr_t va, unsigned npages)
{
	(void) va;
	(void) npages;
	for (unsigned int i = 0; i < npages; i++)
	// {
	// 	paddr_t pa = translate_vaddr(va + (i * PAGE_SIZE));
	// 	//KASSERT(pa != 0);
	// 	vaddr_t kseg0_va = PADDR_TO_KSEG0_VADDR(pa);
	// 	bzero((void *)kseg0_va, PAGE_SIZE);
	// }
	bzero((void *)va, npages * PAGE_SIZE);
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->address_lk = lock_create("address lock");
	if (as->address_lk == NULL)
	{
		kfree(as);
		return NULL;
	}
	
	as->user_heap_start = 0;
	as->user_heap_end = 0;
	as->ptbase = (vaddr_t *)alloc_kpages(1);	// Allocate physical page for the top level page table.
	as_zero_region((vaddr_t)as->ptbase, 1); // Fill the top level page table with zeros
	as->n_kuseg_pages_allocated = 0;


	as->user_stackbase = 0; // Stack will be created later by as_create_stack

	as->user_first_free_vaddr = 0;

	return as;
}



void
as_destroy(struct addrspace *as)
{
    if (as == NULL) {
        return;
    }

    if (as->ptbase != NULL)
    {
        // Iterate over top-level page table entries
        for (int i = 0; i < 1024; i++)
        {
            if (as->ptbase[i] != 0)
            {
                // Extract the low-level page table virtual address by masking out flags
                vaddr_t llpt_entry = as->ptbase[i];
                vaddr_t llpt_vaddr = TLPTE_MASK_VADDR(llpt_entry);
                vaddr_t* llpt = (vaddr_t*)llpt_vaddr;

                // Iterate over low-level page table entries
                for (int j = 0; j < 1024; j++)
                {
                    if (llpt[j] != 0)
                    {
                        // Extract the physical address of the data page
                        vaddr_t llpte_entry = llpt[j];
						if (LLPTE_GET_SWAP_BIT(llpte_entry))
						{
							free_swap_page(llpte_entry);
						}
						else
						{
							paddr_t data_paddr = LLPTE_MASK_PPN(llpte_entry);
							vaddr_t data_vaddr = PADDR_TO_KSEG0_VADDR(data_paddr);

							// Free the data page
							free_kpages(data_vaddr);
						}
                    }
                }

                // Free the low-level page table itself
                free_kpages(llpt_vaddr);
            }
        }

        // Free the top-level page table
        free_kpages((vaddr_t)as->ptbase);
    }

    // Destroy the heap lock if it exists
    if (as->address_lk != NULL) {
        lock_destroy(as->address_lk);
    }

    // Free the address space structure
    kfree(as);

	invalidate_tlb();
}


void
as_activate(void)
{
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	invalidate_tlb();
}
void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	bool in_swap;
	(void)in_swap;
	int npages;
	vaddr_t va = vaddr; // just copy this locally for our own use in the function
	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	// TODO: deallocate all memory from vaddr to vaddr + npages
	int result;
	// bit-shifting so that flags are all 0 or 1 (for PTE)
	result = alloc_upages(as, &va, npages, &in_swap, true, readable >> 2, writeable >> 1, executable);
	if (result)
	{
		return result;
	}
	as->user_first_free_vaddr = va;
	return 0;


}

int
as_prepare_load(struct addrspace *as)
{
	// TODO: remove all pages from previous stack, or just zero them
	// make a new stack for a program that is about to be loaded
	int result = as_create_stack(as);	
	if(result){
		return result;
	}

	/*
	 * All regions should be defined before this function, so at this point
	 * the last available free virtual address for the user space should be 
	 * where the heap starts
	 */
	as->user_heap_start = as->user_first_free_vaddr;
	as->user_heap_end = as->user_first_free_vaddr;


	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	as->user_heap_end = DUMB_HEAP_START;
	as->user_heap_start = DUMB_HEAP_START;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->user_stackbase != 0);

	*stackptr = USERSPACETOP; // For any process the stack starts up and grows down
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{

	lock_acquire(old->address_lk);

	/* 
	 * as_create already creates the first page for for the top level table itself.
	 */
	struct addrspace *new = as_create();
	if (new == NULL)
	{
		lock_release(old->address_lk);
		return ENOMEM; // This might not be the most idicative 
	}

	new->user_heap_start = old->user_heap_start;
	new->user_heap_end = old->user_heap_end;
	new->user_stackbase = old->user_stackbase;

	// try with arrays: 
	for (int i = 0; i < 1024; i++)
	{
		if (old->ptbase[i] != 0) 
		{
			// non-zero value in tlpt. copy llpt then go through it
			vaddr_t *old_as_llpt = (vaddr_t *)TLPTE_MASK_VADDR(old->ptbase[i]);
			vaddr_t *new_as_llpt = (vaddr_t *)alloc_kpages(1); // make it a pointer so we can treat as array
			if (new_as_llpt == 0)
			{
				lock_release(old->address_lk);
				return ENOMEM;
			}
			memcpy(new_as_llpt, old_as_llpt, PAGE_SIZE);
			new->ptbase[i] = (vaddr_t)new_as_llpt | TLPTE_MASK_PAGE_COUNT((vaddr_t)old->ptbase[i]); // put in top-level pagetable
			
			
			for (int j = 0; j < 1024; j++)
			{
				if (old_as_llpt[j] != 0)
				{
					if (LLPTE_GET_SWAP_BIT(old_as_llpt[j]))
					{
						int old_swap_idx = LLPTE_GET_SWAP_OFFSET(old_as_llpt[j]);
						int new_swap_idx = alloc_swap_page(); // add a check here

						read_from_swap(old, old_swap_idx, dumbervm.swap_buffer);
						write_page_to_swap(new, new_swap_idx, dumbervm.swap_buffer);
						new_as_llpt[j] =  LLPTE_SET_SWAP_BIT(new_swap_idx << 12);

					}
					else
					{
						// allocate page, copy data into it, and update llpte
						vaddr_t* old_as_datapage = (vaddr_t *) PADDR_TO_KSEG0_VADDR(LLPTE_MASK_PPN(old_as_llpt[j])); // only PPN
						vaddr_t* new_as_datapage = (vaddr_t *) alloc_kpages(1);
						if (new_as_datapage == 0)
						{
							lock_release(old->address_lk);
							return ENOMEM;
						}
						memcpy(new_as_datapage, old_as_datapage, PAGE_SIZE);	
						paddr_t new_llpte = (paddr_t)(KSEG0_VADDR_TO_PADDR((paddr_t)new_as_datapage) | (old_as_llpt[j] & 0x00000fff)); // NVDG flags
						new_as_llpt[j] = new_llpte;
					}
					new->n_kuseg_pages_allocated++;

				}	
			}
		}
	}

	// At this point both must be equal or we did something wrong
	KASSERT(new->n_kuseg_pages_allocated == old->n_kuseg_pages_allocated); 
	
		
	*ret = new;
	lock_release(old->address_lk);
	return 0;
}

