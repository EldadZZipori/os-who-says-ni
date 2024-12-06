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



/* General VM stuff */

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

void
swap_space_bootstrap(void)
{
	struct vnode* swap_space;
	char swap_space_name[9] = "lhd0raw:";
	int result = vfs_open(swap_space_name,O_RDWR, 0, &swap_space);

	if (result)
	{
		kprintf("dumbervm: no swap space found, continuing without swap space");
		return;
	}

	struct stat swap_space_stat;
	result = VOP_STAT(swap_space, &swap_space_stat);
	 
	if(result)
	{
		vfs_close(swap_space);
		kprintf("dumbervm: cannot query swap space, continuing without swap space");
		return;
	}

	struct bitmap* swap_memlist = bitmap_create((swap_space_stat.st_size)/PAGE_SIZE);
	if (swap_memlist == NULL)
	{
		vfs_close(swap_space);
		kprintf("dumbervm: cannot get swap space size, continuing without swap space");
		return;
	}
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
		// If we get here go check the swap space for more space

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


int 
alloc_upages(struct addrspace* as, vaddr_t* va, unsigned npages, int readable, int writeable, int executable)
{
	(void) readable;
	(void) writeable;
	(void) executable;

	// Allocate memory here
	/*
	* our as_create allocates one page for the top level page table itself.
	*/

	// NOTE: Since we are allocating one block at a time we are not going to have a problem with low level page table getting full 
	// while we are allocating
	uint32_t i;
	for (i = 0; i < npages; i++)
	{
		int vpn1 = VADDR_GET_VPN1(*va);
    	int vpn2 = VADDR_GET_VPN2(*va);
		//bool lastpage = true; // for 


		vaddr_t kseg0_va = alloc_kpages(1);

		// set the 'otherpages' field in the memlist node of the first page in the block

		paddr_t pa = KSEG0_VADDR_TO_PADDR(kseg0_va);          // Physical address of the new block we created.   

		vaddr_t* ll_pagetable_va;
		if (as->ptbase[vpn1] == 0)  // This means the low level page table for this top level page entery was not created yet
		{
			ll_pagetable_va = (vaddr_t *)alloc_kpages(1); // allocate a single page for a low lever page table
			as_zero_region((vaddr_t)ll_pagetable_va, 1); // zero all entries in the new low level page table.

			//as->ptbase[vpn1] = (vaddr_t)((ll_pagetable_va) + 0b1); // Update the count of this low level page table to be 1
			as->ptbase[vpn1] = (vaddr_t)((ll_pagetable_va)); // NOTE: for now no counts too complicated
		} 
		else
		{
			ll_pagetable_va = (vaddr_t *)TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]);
			//as->ptbase[vpn1] += 0b1; // NOTE for now no count too complicated
		}

		// write valid bit
		
		//ll_pagetable_va[vpn2] = pa | (writeable << 10) | (0x1 << 9) | (lastpage << 4) | ((readable << 2) | (writeable << 1) | (executable)); // this will be page aligned

		// temporarily write dirty bit 

		//ll_pagetable_va[vpn2] |= (0x1 << 10) |(readable << 2) | (writeable << 1) ; // for now everything is readable and writeable

		// NOTE: for now just let everything be read/write
		ll_pagetable_va[vpn2] = pa | TLBLO_DIRTY | TLBLO_VALID; 
		
		*va += (vaddr_t)0x1000;
		as->n_kuseg_pages_allocated++;	
	}


	return 0;
}



int 
alloc_heap_upages(struct addrspace* as, int npages)
{
	vaddr_t va = as->user_heap_end;
	int result = alloc_upages(as, &va, npages, 1, 1, 0);
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
		va -= 0x1000;
		free_upages(as, va);
		
	}
	as->user_heap_end = va;
	return 0;
}
/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(unsigned npages)
{
	KASSERT(npages > 0);

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
		unsigned int ppage_index = (paddr - dumbervm.ram_start) / PAGE_SIZE; // Should be page aligned
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
	}
	
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
	invalidate_tlb();
    // if (ts == NULL || ts->ts_vaddr == 0) {
    //     return;
    // }
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	int spl;

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	struct addrspace* as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Tried to access kernel memory */
	if (faultaddress >= MIPS_KSEG0)
	{
		return EFAULT;
	}


	int vpn1 = VADDR_GET_VPN1(faultaddress);

	if(as->ptbase[vpn1] ==  0)	// top level page table was never created, no mapping
	{
		return EFAULT;
	}

	int vpn2 = VADDR_GET_VPN2(faultaddress);
	vaddr_t* ll_pagetable_va = (vaddr_t *) TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]); // the low level page table starts at the address stored in the top level page table entry

	paddr_t ll_pagetable_entry = ll_pagetable_va[vpn2];
	if (ll_pagetable_entry == 0) // there is no entry in the low level page table entry
	{
		return EFAULT;
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
		ll_pagetable_va[vpn2] |= (0b1 << 9); // set bit 9 (valid bit) to 1

		spl = splhigh();

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

		splx(spl);
	}

	
	// why do we need to set the valid bit here?
	// why not set valid bit into PTE when we allocate the page?
	// so that when loaded into TLB, it is already valid
	// and then, we can clear the valid bit when we move to swap
	// which will also do TLB shootdown on the entry.
	//ll_pagetable_va[vpn2] |= (0b1 << 9); // set bit 9 (valid bit) to 1

	// now we know that there is an actual translation in the page tables
	// uint32_t entrylo = (LLPTE_MASK_TLBE(ll_pagetable_va[vpn2])); // entries in the low level page table are aligned with the tlb
	// uint32_t entryhigh = faultaddress;
	
	// spl = splhigh();
	// tlb_random(entryhigh, entrylo); // Just randomly evict for now
	// splx(spl);

	return 0;
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


void 
free_upages(struct addrspace* as, vaddr_t vaddr)
{
	int i = 0;


	/**
	 * Free each page by getting each paddr from vaddr to vaddr + nppages * PAGE_SIZE
	 * and then bitmap free each page
	 * get lastpage bool from llpte
	 */

	while(1)
	{
		vaddr += (i * PAGE_SIZE);
		paddr_t paddr = translate_vaddr_to_paddr(as, vaddr);
		vaddr_t llpte = get_lltpe(as, vaddr);

		free_kpages(PADDR_TO_KSEG0_VADDR(paddr));

		if (LLPTE_GET_LASTPAGE_BIT(llpte))
		{
			// last page. exit
			break;
		}

		vaddr_t* llpt = (vaddr_t *)TLPTE_MASK_VADDR(as->ptbase[VADDR_GET_VPN1(vaddr)]);
		int vpn2 = VADDR_GET_VPN2(vaddr);
		llpt[vpn2] = 0;

		i++;
	}

	// TODO: we need to remove the pagetable entries
}

	
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

	vaddr_t* ll_pagetable_va = (vaddr_t *) TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]);

	return ll_pagetable_va[vpn2];
}