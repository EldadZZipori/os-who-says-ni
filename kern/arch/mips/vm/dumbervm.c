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
#include <cpu.h>


// static
// void
// fill_deadbeef(void *vptr, int npages);

static 
void vm_make_space()
{
	int npages_swapped = 0;
	int temp_swapped = 0;
	unsigned pid_counter = 1; 

	while (npages_swapped < 3 )
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

//static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
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
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}

	struct addrspace* as = proc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}
	spl = splhigh();
	lock_acquire(dumbervm.kern_lk);

	/* Tried to access kernel memory */
	if (faultaddress >= MIPS_KSEG0)
	{
		splx(spl);
		lock_release(dumbervm.kern_lk);
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}

	/* Check top-level page table*/
	int vpn1 = VADDR_GET_VPN1(faultaddress);

	/* Case: This translation does not exist. Fail. */
	if(as->ptbase[vpn1] ==  0)
	{
		splx(spl);
		lock_release(dumbervm.kern_lk);
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}
	/* Case: TLPT is in swap space. Load it in and continue */
	else if (TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]))
	{
		as_load_pagetable_from_swap(as, TLPTE_GET_SWAP_IDX(as->ptbase[vpn1]) , vpn1);
	}

	int vpn2 = VADDR_GET_VPN2(faultaddress);
	vaddr_t* ll_pagetable_va = (vaddr_t *) TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]); // the low level page table starts at the address stored in the top level page table entry

	paddr_t ll_pagetable_entry = ll_pagetable_va[vpn2];
	if (ll_pagetable_entry == 0) // there is no entry in the low level page table entry
	{
		splx(spl);
		lock_release(dumbervm.kern_lk);
		lock_release(dumbervm.fault_lk);
		return EFAULT;
	}
	if (LLPTE_GET_SWAP_BIT(ll_pagetable_entry))
	{
		

			if (as->n_kuseg_pages_ram >= 1 && dumbervm.n_ppages_allocated >= dumbervm.n_ppages)
			{
				replace_ram_page_with_swap_page(as, ll_pagetable_va, vpn2);
			}
			else
			{
				vaddr_t new_ram_page = alloc_kpages(1,false);
				if (new_ram_page == 0)
				{
					splx(spl);
					lock_release(dumbervm.kern_lk);
					lock_release(dumbervm.fault_lk);
					return ENOMEM;
				}
				paddr_t new_ram_page_pa = KSEG0_VADDR_TO_PADDR(new_ram_page);

				int swap_idx = LLPTE_GET_SWAP_OFFSET(ll_pagetable_entry);

				read_from_swap(as, swap_idx, dumbervm.swap_buffer);
				memcpy((void *)new_ram_page, dumbervm.swap_buffer, PAGE_SIZE);
				ll_pagetable_va[vpn2] = new_ram_page_pa | TLBLO_DIRTY | TLBLO_VALID;

				free_swap_page(ll_pagetable_entry);

				ll_pagetable_entry = ll_pagetable_va[vpn2];
				as->n_kuseg_pages_ram++;
				as->n_kuseg_pages_swap--;
			}


		
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

		case VM_FAULT_READONLY:
			KASSERT(LLPTE_GET_DIRTY_BIT(ll_pagetable_entry) == 0);

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


		break;

		case VM_FAULT_READ:
			KASSERT(curproc->p_addrspace != NULL); // TODO: Better way to check this?
			
			entryhi = TLPTE_MASK_VADDR(faultaddress); // kseg0 virtual address of the low level page table
			entrylo = (LLPTE_MASK_TLBE(ll_pagetable_entry)); // entries in the low level page table are aligned with the tlb
			tlb_random(entryhi, entrylo); // load into tlb
			
		break;

		case VM_FAULT_WRITE:

			KASSERT(curproc->p_addrspace != NULL); // TODO: Better way to check this?

			entryhi = TLPTE_MASK_VADDR(faultaddress); // kseg0 virtual address of the low level page table
			entrylo = (LLPTE_MASK_TLBE(ll_pagetable_va[vpn2])); // entries in the low level page table are aligned with the tlb
			tlb_random(entryhi, entrylo); // Just randomly evict for now

		break;

	}

	splx(spl);
	lock_release(dumbervm.kern_lk);
	lock_release(dumbervm.fault_lk);

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
		unsigned int index;
		int result = bitmap_alloc_nbits(dumbervm.ppage_bm, dumbervm.ppage_lastpage_bm ,npages, &index);

		if (!result)
		{
			return dumbervm.ram_start + (PAGE_SIZE * index);
		}

		return 0;
	}
	// When the VM is not ready yet we are just taking stealing memory from the bottom of the ram.
	else
	{
		paddr_t pa = ram_stealmem(npages);
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
	if (kmalloc && dumbervm.swap_buffer!=NULL && curcpu->c_spinlocks !=1)
	{
		lock_acquire(dumbervm.kern_lk);
	}
	if (dumbervm.n_ppages_allocated >= dumbervm.n_ppages -5)
	{
		vm_make_space();
	}

	paddr_t pa = getppages(npages);

	if (pa == 0) {
		if (kmalloc && dumbervm.swap_buffer!=NULL&& curcpu->c_spinlocks !=1)
		{
			lock_release(dumbervm.kern_lk);
		}
		return 0;
	}

	if (pa % PAGE_SIZE != 0)
	{
		if (kmalloc && dumbervm.swap_buffer!=NULL&& curcpu->c_spinlocks !=1)
		{
			lock_release(dumbervm.kern_lk);
		}
		return pa;
	}

	/* No memlist required */
	vaddr_t va = PADDR_TO_KSEG0_VADDR(pa);

	as_zero_region(va, 1);

	KASSERT(va >= MIPS_KSEG0);
	KASSERT(va < MIPS_KSEG0_RAM_END);

	if (kmalloc && dumbervm.swap_buffer!=NULL&& curcpu->c_spinlocks !=1)
	{
		lock_release(dumbervm.kern_lk);
	}
	return va;

}

void
free_kpages(vaddr_t addr, bool is_kfree)
{
	KASSERT(addr >= MIPS_KSEG0);
	KASSERT(addr < MIPS_KSEG0_RAM_END);
	(void)is_kfree;
	// if (is_kfree && dumbervm.swap_buffer!=NULL)
	// {
	// 	lock_acquire(dumbervm.kern_lk);
	// }
	paddr_t paddr = addr - MIPS_KSEG0;

	// For now, just remove a single page.
	//memlist_remove(dumbervm.ppage_memlist, paddr);
	if ((paddr) % PAGE_SIZE == 0)
	{
		unsigned int ppage_index = ((paddr - dumbervm.ram_start) / PAGE_SIZE ); // Should be page aligned
		//spinlock_acquire(&dumbervm.ppage_bm_sl);

		if (bitmap_isset(dumbervm.ppage_lastpage_bm, ppage_index)) // This is a single allocation
		{
			bitmap_unmark(dumbervm.ppage_bm, ppage_index);
			dumbervm.n_ppages_allocated--;
			bitmap_unmark(dumbervm.ppage_lastpage_bm, ppage_index);
			//spinlock_release(&dumbervm.ppage_bm_sl);
			// if (is_kfree && dumbervm.swap_buffer!=NULL)
			// {
			// 	lock_release(dumbervm.kern_lk);
			// }
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
		
		//spinlock_release(&dumbervm.ppage_bm_sl);

		//fill_deadbeef((void * )addr, 1);

	}
	// if (is_kfree && dumbervm.swap_buffer!=NULL)
	// {
	// 	lock_release(dumbervm.kern_lk);
	// }
}

/* User Page Managment */
int 
alloc_upages(struct addrspace* as, vaddr_t* va, unsigned npages ,bool* in_swap, int readable, int writeable, int executable)
{
	(void) readable;
	(void) writeable;
	(void) executable;

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

		vaddr_t* ll_pagetable_va;
		if (as->ptbase[vpn1] == 0)  // This means the low level page table for this top level page entery was not created yet
		{
			ll_pagetable_va = (vaddr_t *)alloc_kpages(1,false); // allocate a single page for a low lever page table
			as_zero_region((vaddr_t)ll_pagetable_va, 1); // zero all entries in the new low level page table.

			as->ptbase[vpn1] = (vaddr_t)((ll_pagetable_va)); // NOTE: for now no counts too complicated
		}  
		else
		{
			if(TLPTE_GET_SWAP_BIT(as->ptbase[vpn1]))
			{
				as_load_pagetable_from_swap(as, TLPTE_GET_SWAP_IDX(as->ptbase[vpn1]),vpn1);
			}
			ll_pagetable_va = (vaddr_t *)TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]);
		}
		vaddr_t kseg0_va;
		paddr_t pa;          // Physical address of the new block we created.

		*in_swap = false;
		kseg0_va = alloc_kpages(1, false);
		if (kseg0_va == 0)
		{
			lock_release(dumbervm.kern_lk);
			return ENOMEM;
		}
		pa = KSEG0_VADDR_TO_PADDR(kseg0_va);
		pa |= TLBLO_DIRTY | TLBLO_VALID | executable;
		as->n_kuseg_pages_ram++;

		ll_pagetable_va[vpn2] = pa ; 
		

		*va += (vaddr_t)PAGE_SIZE;
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
		va -= PAGE_SIZE;
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
		 free_kpages(PADDR_TO_KSEG0_VADDR(paddr),false);
		llpt[vpn2] = 0;
		as->n_kuseg_pages_ram--;
	}

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


// static
// void
// fill_deadbeef(void *vptr, int npages)
// {
// 	uint32_t *ptr = vptr;
// 	size_t i;

// 	for (i=0; i<(npages * PAGE_SIZE)/sizeof(uint32_t); i++) {
// 		ptr[i] = 0xdeadbeef;
// 	}
// }
