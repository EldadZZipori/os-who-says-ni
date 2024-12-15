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
#include <uio.h>
#include <kern/swapspace.h>
#include <proctable.h>
#include <kern/swapspace.h>


static
void
fill_deadbeef(void *vptr, int npages);

static 
void vm_make_space()
{
	int npages_swapped = 0;
	int temp_swapped = 0;
	unsigned pid_counter = 1; 
	//while (npages_swapped < 3 && (pid_counter < kproc_table->process_counter))
	while (npages_swapped < 1 )
	{
		if ((unsigned)curproc->p_pid != pid_counter)
		{
			if (kproc_table->processes[pid_counter] != NULL && kproc_table->processes[pid_counter]->p_addrspace != NULL)
			{
				as_move_to_swap(kproc_table->processes[pid_counter]->p_addrspace, 50, &temp_swapped);
				npages_swapped+= temp_swapped;
			}
		}

		pid_counter++;
		if (pid_counter >= kproc_table->process_counter) pid_counter = 1;
	}

}
/* Virtual Machine */

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
void
vm_bootstrap(void)
{
	/*
	 * 1. Fetch bottom of ram
	 * 2. Fetch size of ram
	 * 3. Init a bitmap on stolen first page
	 */
	paddr_t ram_end = ram_getsize();
	paddr_t ppages_bm_start = ram_getfirstfree(); // when this is called stealram is unavailable

	paddr_t tracked_ram_start = ppages_bm_start + PAGE_SIZE;
	tracked_ram_start = (tracked_ram_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); // align to first page after the bitmap

	unsigned int n_ppages = (ram_end - tracked_ram_start) / PAGE_SIZE;
	dumbervm.n_ppages = n_ppages;

	paddr_t ppage_lastpage_bm_start = bitmap_bootstrap(ppages_bm_start, n_ppages); // the return of this function is ram_start itself
	
	dumbervm.ppage_bm = (struct bitmap*)PADDR_TO_KSEG0_VADDR(ppages_bm_start);

	bitmap_bootstrap(ppage_lastpage_bm_start, n_ppages);

	dumbervm.ppage_lastpage_bm = (struct bitmap*)PADDR_TO_KSEG0_VADDR(ppage_lastpage_bm_start);

	dumbervm.ram_start = tracked_ram_start;

	spinlock_init(&dumbervm.ppage_bm_sl);

	dumbervm.vm_ready = true;
	
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	int spl;
	lock_acquire(dumbervm.fault_lk);

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		panic("\n1\n");
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}

	struct addrspace* as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		panic("\n2\n");

		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}
	spl = splhigh();
	/* Tried to access kernel memory */
	if (faultaddress >= MIPS_KSEG0)
	{
		panic("\n3\n");
		splx(spl);
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}

	/* Check top-level page table*/
	int vpn1 = VADDR_GET_VPN1(faultaddress);

	/* Case: This translation does not exist. Fail. */
	if(as->ptbase[vpn1] ==  0)
	{
		panic("\n4\n");
		splx(spl);
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}
	/* Case: TLPT is in swap space. Load it in and continue */
	else if (TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]))
	{
		lock_acquire(dumbervm.kern_lk);
		as_load_pagetable_from_swap(as, TLPTE_GET_SWAP_IDX(as->ptbase[vpn1]) , vpn1);
		lock_release(dumbervm.kern_lk);
	}

	int vpn2 = VADDR_GET_VPN2(faultaddress);
	vaddr_t* ll_pagetable_va = (vaddr_t *) TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]); // the low level page table starts at the address stored in the top level page table entry

	paddr_t ll_pagetable_entry = ll_pagetable_va[vpn2];
	if (ll_pagetable_entry == 0) // there is no entry in the low level page table entry
	{
		panic("\n6\n");
		splx(spl);
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}

	if (LLPTE_GET_SWAP_BIT(ll_pagetable_entry))
	{
		// Move the swap page into RAM. 

		//const int n_proc_allowable_ram_pages = 7;
		//const int n_min_free_ram_pages_global = 10;
		lock_acquire(dumbervm.kern_lk);

		// Case 1: This address space already hit its max RAM pages (around 7)
		// What we should do: 
		// Swap this new page into RAM and put another page in this AS into swap
		// 1. Find a page in RAM that we can swap
		// 2. Write that RAM page to a swap page
		// 3. Move the page already in swap (faultaddress) into RAM
		// 4. Free that swap page 
		// 5. Update low level page table entries for both pages
		// if (as->n_kuseg_pages_ram >= n_proc_allowable_ram_pages)
		// {
		// 	bool *did_find;
		// 	vaddr_t swappable_ram_page = find_swapable_page(as, &did_find, false);
		// 	if (!did_find)
		// 	{
		// 		lock_release(dumbervm.kern_lk);
		// 		splx(spl);
		// 		lock_release(dumbervm.fault_lk);
		// 		return ENOMEM; // should have passed this since we have free RAM pages
		// 	}
		// 	int swap_idx = LLPTE_GET_SWAP_OFFSET(ll_pagetable_entry);
		// 	read_from_swap(as, swap_idx, dumbervm.swap_buffer);

		// 	// write the data in the ram page to the swap page
		// 	// NOTE: Not sure if this vaddr is the correct format
		// 	write_page_to_swap(as, swap_idx, (void *)swappable_ram_page);

		// 	memcpy((void *)swappable_ram_page, dumbervm.swap_buffer, PAGE_SIZE);

		// 	// update the low level page table entries, not sure if this is right
		// 	ll_pagetable_va[vpn2] = LLPTE_UNSET_SWAP_BIT(ll_pagetable_entry); // remove the swap bit
		// 	ll_pagetable_va[vpn2] = LLPTE_MASK_PPN(swappable_ram_page); // set the physical page number
	
		// }

		// // Case 2: Dumbervm has > 10 (or whatever the amount is) free pages left in RAM 
		// // What we should do: 
		// // Allocate a ppage and map this vaddr to it
		// else if (dumbervm.n_ppages_allocated < n_min_free_ram_pages_global)
		// {
		// 	// Allocate a page
		// 	vaddr_t kpage = alloc_kpages(1);
		// 	if (kpage == 0)
		// 	{
		// 		lock_release(dumbervm.kern_lk);
		// 		splx(spl);
		// 		lock_release(dumbervm.fault_lk);
		// 		return ENOMEM; // should have passed this since we have free RAM pages
		// 	}

		// 	// update the page table entries 
		// 	// Before: [   SWAP_IDX    | NVDL and other bits ]
		// 	// After:  [   PPN         | NVLD adn other bits (swap is now 0)]
		// 	ll_pagetable_entry = (kpage)
		// }

		// if (as->n_kuseg_pages_ram >= n_proc_allowable_ram_pages)
		// {
		// 	// just swap page
		// 	vaddr_t old_ram_va = replace_ram_page_with_swap_page(as, ll_pagetable_va, vpn2);
		// 	int idx = tlb_probe(old_ram_va, 0);

		// 	if (idx >= 0)
		// 	{
		// 		tlb_write(TLBHI_INVALID(idx),TLBLO_INVALID(), idx);
		// 	}
		// 	ll_pagetable_entry = ll_pagetable_va[vpn2];

		// }
		// else if (dumbervm.n_ppages_allocated < dumbervm.n_ppages-n_min_free_ram_pages_global)
		// {
			vaddr_t new_ram_page = alloc_kpages(1,false);
			if (new_ram_page == 0)
			{
				panic("\n8\n");
				lock_release(dumbervm.kern_lk);
				splx(spl);
				lock_release(dumbervm.fault_lk);
				return ENOMEM;
			}
			paddr_t new_ram_page_pa = KSEG0_VADDR_TO_PADDR(new_ram_page);

			int swap_idx = LLPTE_GET_SWAP_OFFSET(ll_pagetable_entry);

			read_from_swap(as, swap_idx, dumbervm.swap_buffer);
			memcpy((void *)new_ram_page, dumbervm.swap_buffer, PAGE_SIZE);
			ll_pagetable_va[vpn2] = new_ram_page_pa | TLBLO_DIRTY | TLBLO_VALID;
			ll_pagetable_entry = ll_pagetable_va[vpn2];

		//}
		// else
		// {
		// 	vm_make_space();

		// 	vaddr_t new_ram_page = alloc_kpages(1);
		// 	if (new_ram_page == 0)
		// 	{
		// 		panic("\n11\n");
		// 		lock_release(dumbervm.kern_lk);
		// 		splx(spl);
		// 		lock_release(dumbervm.fault_lk);
		// 		return ENOMEM;
		// 	}
		// 	paddr_t new_ram_page_pa = KSEG0_VADDR_TO_PADDR(new_ram_page);

		// 	int swap_idx = LLPTE_GET_SWAP_OFFSET(ll_pagetable_entry);

		// 	read_from_swap(as, swap_idx, dumbervm.swap_buffer);
		// 	memcpy((void *)new_ram_page, dumbervm.swap_buffer, PAGE_SIZE);
		// 	ll_pagetable_va[vpn2] = new_ram_page_pa | TLBLO_DIRTY | TLBLO_VALID;
		// 	ll_pagetable_entry = ll_pagetable_va[vpn2];
		// }

		// Case 3: Address space not at its max RAM pages but dumbervm has < 10 free pages left in RAM
		// What we should do: 
		// There is no space in RAM to put this, so steal a RAM page from another process
		// => While we haven't allocated 7 (or whatever the amount is) RAM pages, 
		//    walk the proctable and move them to swap space

		lock_release(dumbervm.kern_lk);
	}
	
	/** 
	 * VM_FAULT_READONLY: Attempted to write to a TLB entry whose dirty bit is not set
	 * happens when: 
	 * 	  - kernel sets up process: loading non-writeable page from disk (load segment)
	 *    - IMPORTANT: if we set dirty bit during load_elf, user should never see this fault
	 * what we should do: 
	 *   - set dirty bit only in TLB, not in PTE
	 *   - set loaded bit in low level page table
	 * 
	 * VM_FAULT_READ: Attempted to read from a page not present in the TLB
	 * happens when:
	 *    - user runtime: TLB miss
	 * what we should do:
	 *   - put in TLB
	 * 
	 * VM_FAULT_WRITE: Attempted to write to a page not present in the TLB
	 * happens when:
	 *    - user runtime: TLB miss
	 * what we should do:
	 *  - put in TLB
	*/
	switch (faulttype)
	{
		
		uint32_t entrylo, entryhi;

		// FOR DEBUGGING PURPOSES ONLY
		// Entry should be valid after we are done with it
		//ll_pagetable_va[vpn2] |= (0b1 << 9); // set bit 9 (valid bit) to 1


		case VM_FAULT_READONLY:

			// assert we are kernel 
			// assert dirty bit is not set (since we are here)
			// assert this page is non-writeable
			// assert this page is not loaded
			//KASSERT(curproc->p_addrspace == NULL); // TODO: Better way to check this?
			KASSERT(LLPTE_GET_DIRTY_BIT(ll_pagetable_entry) == 0);

			// NOTE: for now don't use permission bits at all
			//KASSERT(LLPTE_GET_WRITE_PERMISSION_BIT(ll_pagetable_entry) == 0);
			//KASSERT(LLPTE_GET_LOADED_BIT(ll_pagetable_entry) == 0);
			
			// 1. set dirty bit in TLB
			// spl = splhigh();
			int idx = tlb_probe(faultaddress, 0);
			if (idx >= 0)
			{ 
				tlb_read(&entryhi, &entrylo, idx);
				//entrylo |= (0b1 << 10); // set dirty bit
				tlb_write(entryhi, entrylo, idx);
			} 
			else
			{
				entryhi = TLPTE_MASK_VADDR(faultaddress); // kseg0 virtual address of the low level page table
				entrylo = (LLPTE_MASK_TLBE(ll_pagetable_entry)); // entries in the low level page table are aligned with the tlb
				//entrylo |= (0b1 << 10); // set dirty bit
				tlb_random(entryhi, entrylo); // overwrite random entry
			}
			// splx(spl);

			// 2. set loaded bit in low level page table
			//ll_pagetable_va[vpn2] |= (0b1 << 5); // set bit 5 (loaded) to 1

		break;

		case VM_FAULT_READ:
			// assert we are a user 
			// assert this page is indeed not in the TLB
			// spl = splhigh();
			KASSERT(curproc->p_addrspace != NULL); // TODO: Better way to check this?
			int index = tlb_probe(faultaddress, 0);
			if (index >=0 )
			{
				KASSERT(index < 0);
			}
			

			// NOTE: for now dont use permission bits at all
			// if(!LLPTE_GET_READ_PERMISSION_BIT(ll_pagetable_entry))
			// {
			// 	return EFAULT; // tried to read from a non readable page
			// }

			// add to tlb
			entryhi = TLPTE_MASK_VADDR(faultaddress); // kseg0 virtual address of the low level page table
			entrylo = (LLPTE_MASK_TLBE(ll_pagetable_entry)); // entries in the low level page table are aligned with the tlb
			tlb_random(entryhi, entrylo); // load into tlb
			// splx(spl);
			
		break;

		case VM_FAULT_WRITE:
			// assert we are a user
			// assert this page is indeed not in the TLB
			KASSERT(curproc->p_addrspace != NULL); // TODO: Better way to check this?
			KASSERT(tlb_probe(faultaddress, 0) < 0);

			// NOTE: for now don't use permission bits at all
			// if(!LLPTE_GET_WRITE_PERMISSION_BIT(ll_pagetable_entry) && LLPTE_GET_LOADED_BIT(ll_pagetable_entry))
			// {
			// 	return EFAULT;
			// }
			// add to tlb
			entryhi = TLPTE_MASK_VADDR(faultaddress); // kseg0 virtual address of the low level page table
			entrylo = (LLPTE_MASK_TLBE(ll_pagetable_va[vpn2])); // entries in the low level page table are aligned with the tlb
			tlb_random(entryhi, entrylo); // Just randomly evict for now

		break;

	}

	splx(spl);
	lock_release(dumbervm.fault_lk);
	
	// why do we need to set the valid bit here?
	// why not set valid bit into PTE when we allocate the page?
	// so that when loaded into TLB, it is already valid
	// and then, we can clear the valid bit when we move to swap
	// which will also do TLB shootdown on the entry.
	//ll_pagetable_va[vpn2] |= (0b1 << 9); // set bit 9 (valid bit) to 1

	return 0;
}

void
vm_tlbshootdown_all(void)
{
	invalidate_tlb();
    return;
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	//invalidate_tlb();
    if (ts == NULL || ts->va == 0) {
        return;
    }

	int idx = tlb_probe(ts->va, 0);
	if (idx >= 0)
	{
		tlb_write(TLBHI_INVALID(idx), TLBLO_INVALID(), idx);
	}

}


void 
invalidate_tlb(void)
{
	int i, spl,idx;

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		idx = tlb_probe(TLBHI_INVALID(i),0);
		if (idx <0)
		{
			tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
		}
		
	}

	splx(spl);
}

static
paddr_t
getppages(unsigned long npages)
{
	KASSERT(npages > 0);
	if (dumbervm.vm_ready)
	{
		// get first free physical page 
		// no mem left
		unsigned int index;
		spinlock_acquire(&dumbervm.ppage_bm_sl);
		int result = bitmap_alloc_nbits(dumbervm.ppage_bm, dumbervm.ppage_lastpage_bm ,npages, &index);
		spinlock_release(&dumbervm.ppage_bm_sl);

		if (!result)
		{
			return dumbervm.ram_start + (0x1000 * index);
		}

		return 0;
	}
	else
	{
		spinlock_acquire(&stealmem_lock);

		paddr_t pa = ram_stealmem(npages);

		spinlock_release(&stealmem_lock);

		return pa;
	}

}

/**
 * @brief Allocates a page for the kernel. This allocates continuous pages. 
 * 
 * @param npages number of pages to be allocated
 * 
 * @return the virtual address (KSEG0) of the first allocated page.
 */
vaddr_t
alloc_kpages(unsigned npages, bool kmalloc)
{
	KASSERT(npages > 0);
	if (kmalloc && dumbervm.swap_buffer != NULL)
	{
		//kprintf("\nkmalloc\n");
	}
	if (dumbervm.n_ppages_allocated >= dumbervm.n_ppages -5)
	{
		if (kmalloc && dumbervm.swap_buffer!=NULL)
		{
			lock_acquire(dumbervm.kern_lk);
		}
		vm_make_space();
		if (kmalloc && dumbervm.swap_buffer!=NULL)
		{
			lock_release(dumbervm.kern_lk);
		}
	}

	paddr_t pa = getppages(npages);

	if (pa == 0) {
		return 0;
	}

	if (pa % PAGE_SIZE != 0)
	{
		return pa;
	}

	/* No memlist required */
	vaddr_t va = PADDR_TO_KSEG0_VADDR(pa);

	as_zero_region(va, 1);

	KASSERT(va >= MIPS_KSEG0);
	KASSERT(va < MIPS_KSEG0_RAM_END);

	return va;

}

void
free_kpages(vaddr_t addr)
{
	KASSERT(addr >= MIPS_KSEG0);
	KASSERT(addr < MIPS_KSEG0_RAM_END);

	paddr_t paddr = addr - MIPS_KSEG0;

	// For now, just remove a single page.
	//memlist_remove(dumbervm.ppage_memlist, paddr);
	if ((paddr) % PAGE_SIZE == 0)
	{
		unsigned int ppage_index = ((paddr - dumbervm.ram_start) / PAGE_SIZE ); // Should be page aligned
		spinlock_acquire(&dumbervm.ppage_bm_sl);

		if (bitmap_isset(dumbervm.ppage_lastpage_bm, ppage_index)) // This is a single allocation
		{
			bitmap_unmark(dumbervm.ppage_bm, ppage_index);
			dumbervm.n_ppages_allocated--;
			bitmap_unmark(dumbervm.ppage_lastpage_bm, ppage_index);
			spinlock_release(&dumbervm.ppage_bm_sl);
			return;

		}
		while (!bitmap_isset(dumbervm.ppage_lastpage_bm, ppage_index)) // Deallocate untill seeing a 1
		{
			bitmap_unmark(dumbervm.ppage_bm, ppage_index);
			dumbervm.n_ppages_allocated--;
			ppage_index++;
		}

		bitmap_unmark(dumbervm.ppage_bm, ppage_index);	// deallocate the 1
		dumbervm.n_ppages_allocated--;
		bitmap_unmark(dumbervm.ppage_lastpage_bm, ppage_index);
		
		spinlock_release(&dumbervm.ppage_bm_sl);

		fill_deadbeef((void * )addr, 1);

	}
	
}

/* User Page Managment */
int 
alloc_upages(struct addrspace* as, vaddr_t* va, unsigned npages ,bool* in_swap, int readable, int writeable, int executable)
{
	(void) readable;
	(void) writeable;
	(void) executable;

	// Allocate memory here
	/*
	* our as_create allocates one page for the top level page table itself.
	*/
	lock_acquire(dumbervm.kern_lk);

	// NOTE: Since we are allocating one block at a time we are not going to have a problem with low level page table getting full 
	// while we are allocating
	uint32_t i;
	for (i = 0; i < npages; i++)
	{
		int vpn1 = VADDR_GET_VPN1(*va);
    	int vpn2 = VADDR_GET_VPN2(*va);
		//bool lastpage = true; // for 


		//vaddr_t kseg0_va = alloc_kpages(1);

		// set the 'otherpages' field in the memlist node of the first page in the block

		//paddr_t pa = KSEG0_VADDR_TO_PADDR(kseg0_va);          // Physical address of the new block we created.   
		//bool lastpage = true; // for 
   

		vaddr_t* ll_pagetable_va;
		if (as->ptbase[vpn1] == 0)  // This means the low level page table for this top level page entery was not created yet
		{
			ll_pagetable_va = (vaddr_t *)alloc_kpages(1,false); // allocate a single page for a low lever page table
			as_zero_region((vaddr_t)ll_pagetable_va, 1); // zero all entries in the new low level page table.

			//as->ptbase[vpn1] = (vaddr_t)((ll_pagetable_va) + 0b1); // Update the count of this low level page table to be 1
			as->ptbase[vpn1] = (vaddr_t)((ll_pagetable_va)); // NOTE: for now no counts too complicated
		}  
		else
		{
			if(TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]))
			{
				as_load_pagetable_from_swap(as, TLPTE_GET_SWAP_IDX(as->ptbase[vpn1]),vpn1);
			}
			ll_pagetable_va = (vaddr_t *)TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]);
			//as->ptbase[vpn1] += 0b1; // NOTE: dont use these for now
		}
		//vaddr_t kseg0_va;
		

		// set the 'otherpages' field in the memlist node of the first page in the block

		paddr_t pa;          // Physical address of the new block we created.
		// if (as->n_kuseg_pages_ram >= 7 && !executable) // in this case we should allocate memory from the swap space
		// {
			*in_swap = true;
			int swap_idx = alloc_swap_page(); // find free area in swap space
			pa = LLPTE_SET_SWAP_BIT(swap_idx << 12);
			as->n_kuseg_pages_swap++;
		// }
		// else
		// {
		// 	*in_swap = false;
		// 	kseg0_va = alloc_kpages(1);
		// 	if (kseg0_va == 0)
		// 	{
		// 		lock_release(dumbervm.kern_lk);
		// 		return ENOMEM;
		// 	}
		// 	pa = KSEG0_VADDR_TO_PADDR(kseg0_va);
		// 	pa |= TLBLO_DIRTY | TLBLO_VALID | executable;
		// 	as->n_kuseg_pages_ram++;
		// }

		// write valid bit
		
		//ll_pagetable_va[vpn2] = pa | (writeable << 10) | (0x1 << 9) | (lastpage << 4) | ((readable << 2) | (writeable << 1) | (executable)); // this will be page aligned

		// temporarily write dirty bit 

		//ll_pagetable_va[vpn2] |= (0x1 << 10) |(readable << 2) | (writeable << 1) ; // for now everything is readable and writeable

		// NOTE: for now just let everything be read/write
		ll_pagetable_va[vpn2] = pa ; 
		
		//ll_pagetable_va[vpn2] |= (0x1 << 10) | (0b1 << 2) | (0b1 << 1); // for now everything is readable and writeable

		*va += (vaddr_t)0x1000;
	}
	lock_release(dumbervm.kern_lk);

	return 0;
}



int 
alloc_heap_upages(struct addrspace* as, int npages)
{
	bool in_swap;
	(void)in_swap;
	vaddr_t va = as->user_heap_end;
	int result = alloc_upages(as, &va, npages, &in_swap,1, 1, 0);
	if (result)
	{
		return result;
	}

	as->user_heap_end = (vaddr_t) va;

	return 0;
}
int 
free_heap_upages(struct addrspace* as, int npages)
{
	vaddr_t va = as->user_heap_end;
	for (int i = 0; i < npages; i++)
	{
		// deallocating happens by zeroing from the bottom up so we need to decrease the address
		va -= 0x1000;
		free_upages(as, va);
		
	}
	as->user_heap_end = va;
	return 0;
}

void 
free_upages(struct addrspace* as, vaddr_t vaddr)
{
	//int i = 0;
	lock_acquire(dumbervm.kern_lk);

	/**
	 * Free each page by getting each paddr from vaddr to vaddr + nppages * PAGE_SIZE
	 * and then bitmap free each page
	 * get lastpage bool from llpte
	 */

	// NOTE: for now no lastpage bit, it is unnessaery here because we only come here from sbrk or when we deallocate all the addrspace
	// while(1)
	// {
	paddr_t paddr = translate_vaddr_to_paddr(as, vaddr); // to free stuff we have to do it from the bottom
	vaddr_t llpte = get_lltpe(as, vaddr);

	int vpn1 = VADDR_GET_VPN1(vaddr);
	if(TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]))
	{
		as_load_pagetable_from_swap(as, TLPTE_GET_SWAP_IDX(as->ptbase[vpn1]),vpn1);
	}
	int vpn2 = VADDR_GET_VPN2(vaddr);
	vaddr_t* llpt = (vaddr_t *)as->ptbase[vpn1];


	if (LLPTE_GET_SWAP_BIT(llpte))
	{
		free_swap_page(llpte);
		llpt[vpn2] = 0;
		as->n_kuseg_pages_swap--;
	}
	else
	{
		// basiclly just make the entry not valid so we dont free the physical page which we need
		 free_kpages(PADDR_TO_KSEG0_VADDR(paddr));
		//paddr = paddr & ~TLBLO_VALID;
		//llpt[vpn2] = paddr;
		llpt[vpn2] = 0;
		as->n_kuseg_pages_ram--;
	}
	// if (LLPTE_GET_LASTPAGE_BIT(llpte))
	// {
	// 	// last page. exit
	// 	break;
	// }

	// vaddr_t* llpt = (vaddr_t *)TLPTE_MASK_VADDR(as->ptbase[VADDR_GET_VPN1(vaddr)]);
	// int vpn2 = VADDR_GET_VPN2(vaddr);
	// llpt[vpn2] = 0;

	// i++;
	//}

	// TODO: we need to remove the pagetable entries
	lock_release(dumbervm.kern_lk);
}

	
/* Helper Functions */
		
paddr_t
translate_vaddr_to_paddr(struct addrspace* as, paddr_t vaddr)
{
	int vpn1 = VADDR_GET_VPN1(vaddr);
	int vpn2 = VADDR_GET_VPN2(vaddr);

	if (as == NULL) {
		return 0;
	}

	KASSERT(as->ptbase != NULL); // top-level page table must be created

	if(as->ptbase[vpn1] ==  0)	// lower-level page table was never created, no mapping
	{
		return 0;
	}
	if(TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]))
	{
		as_load_pagetable_from_swap(as, TLPTE_GET_SWAP_IDX(as->ptbase[vpn1]),vpn1);
	}
	vaddr_t* ll_pagetable_va = (vaddr_t *) TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]);

	if (ll_pagetable_va[vpn2] == 0) // there is no entry in the low level page table entry
	{
		return 0;
	}

	// return [      PPAGE      | OFFSET ]
	return (paddr_t) (LLPTE_MASK_PPN(ll_pagetable_va[vpn2]) | VADDR_GET_OFFSET(vaddr));
}

vaddr_t 
get_lltpe(struct addrspace* as,vaddr_t vaddr)
{
	int vpn1 = VADDR_GET_VPN1(vaddr);
	int vpn2 = VADDR_GET_VPN2(vaddr);

	if (as == NULL) {
		return 0;
	}

	KASSERT(as->ptbase != NULL); // top-level page table must be created

	if(as->ptbase[vpn1] ==  0)	// lower-level page table was never created, no mapping
	{
		return 0;
	}
	if(TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]))
	{
		as_load_pagetable_from_swap(as, TLPTE_GET_SWAP_IDX(as->ptbase[vpn1]),vpn1);
	}

	vaddr_t* ll_pagetable_va = (vaddr_t *) TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]);

	return ll_pagetable_va[vpn2];
}


static
void
fill_deadbeef(void *vptr, int npages)
{
	uint32_t *ptr = vptr;
	size_t i;

	for (i=0; i<(npages * PAGE_SIZE)/sizeof(uint32_t); i++) {
		ptr[i] = 0xdeadbeef;
	}
}

/**
 * @brief move a lower-level page table to swap space
 * 
 * @param llpt lower-level page table (ptbase[i] suffices)
 * 
 * Replaces the llpt address (kseg0 vaddr) with the swap space index
 */
 
/**
 * @brief move a lower-level page table to swap space
 * 
 * @param llpt lower-level page table (ptbase[i] suffices)
 * 
 * Replaces the llpt address (kseg0 vaddr) with the swap space index
 */

int
as_move_pagetable_to_swap(struct addrspace* as, int vpn1)
{
	// Move the lower-level page table to swap space
	// This is used when we are about to run out of memory
	// We will move the lower-level page table to swap space
	KASSERT(TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]) == 0); // should not be in swap space already

	// move it to swap space 
	int swap_idx = alloc_swap_page(); 
	if (swap_idx == -1) 
	{ 
		panic("\n9\n");
		return swap_idx; 
	}
	// buf should be a kseg0 vaddr?
	write_page_to_swap(as, swap_idx, (void *)TLPTE_MASK_VADDR(as->ptbase[vpn1])); 
	free_kpages(as->ptbase[vpn1]);
	// Update the top-level page table entry to point to the swap space
	as->ptbase[vpn1] = (vaddr_t)(swap_idx << 12 | 0b1); // set the swap bit	 
	

	return 0; 
}

/** 
 * @brief 
 * 
 * @param swap_idx swap page number 
 * @param vpn1 index into top-level page table to move this into
 * 
 * Allocates a kpage to move the swap data into RAM 
 * 
 * updates the tlpte with the new paddr. 
 * 
 **/
 
int
as_load_pagetable_from_swap(struct addrspace *as, int swap_idx, int vpn1)
{
	KASSERT(TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]) == 1);

	// not grabbing kern lock because we should already have it. 
	
	vaddr_t new_ram_page = alloc_kpages(1,false);
	if (new_ram_page == 0)
	{
		panic("\n5\n");
		return ENOMEM;
	}

	read_from_swap(as, swap_idx, dumbervm.swap_buffer);
	memcpy((void *)new_ram_page, dumbervm.swap_buffer, PAGE_SIZE);
	// tlpte is now [  kseg0 vaddr of llpt | swap bit (zero in this case) ]
	free_swap_page(as->ptbase[vpn1]);
	as->ptbase[vpn1] = new_ram_page; 

	return 0;
}