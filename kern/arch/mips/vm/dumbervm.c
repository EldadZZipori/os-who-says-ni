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

#define DUMBVMER_STACKPAGES    8


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


static
int 
alloc_upages(struct addrspace* as, vaddr_t* va, unsigned npages, int readable, int writeable, int executable)
{

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
		bool lastpage = (i == npages - 1);


		vaddr_t kseg0_va = alloc_kpages(1);

		// set the 'otherpages' field in the memlist node of the first page in the block

		paddr_t pa = KSEG0_VADDR_TO_PADDR(kseg0_va);          // Physical address of the new block we created.   

		vaddr_t* ll_pagetable_va;
		if (as->ptbase[vpn1] == 0)  // This means the low level page table for this top level page entery was not created yet
		{
			ll_pagetable_va = (vaddr_t *)alloc_kpages(1); // allocate a single page for a low lever page table
			as_zero_region((vaddr_t)ll_pagetable_va, 1); // zero all entries in the new low level page table.

			as->ptbase[vpn1] = (vaddr_t)((ll_pagetable_va) + 0b1); // Update the count of this low level page table to be 1
		} 
		else
		{
			ll_pagetable_va = (vaddr_t *)TLPTE_MASK_VADDR((vaddr_t)as->ptbase[vpn1]);
			as->ptbase[vpn1] += 0b1;
		}

		// write valid bit
		
		ll_pagetable_va[vpn2] = pa | (writeable << 10) | (0x1 << 9) | (lastpage << 4) | ((readable << 2) | (writeable << 1) | (executable)); // this will be page aligned

		// temporarily write dirty bit 

		ll_pagetable_va[vpn2] |= (0x1 << 10);

		*va += (vaddr_t)0x1000;
		as->n_kuseg_pages_allocated++;	
	}


	return 0;
}

static
int 
as_create_stack(struct addrspace* as)
{
	vaddr_t stack_va = USERSPACETOP - (DUMBVMER_STACKPAGES * PAGE_SIZE);
	int result = alloc_upages(as, &stack_va, DUMBVMER_STACKPAGES, 1,1,0); 
	if (result)
	{
		return result;
	}
	vaddr_t stacktop = stack_va;
	if (stacktop != USERSPACETOP) {
		return ENOMEM;
	}
	as->user_stackbase = USERSPACETOP - (DUMBVMER_STACKPAGES * PAGE_SIZE);
	as_zero_region(as->user_stackbase, DUMBVMER_STACKPAGES);

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
    return;
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
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
			KASSERT(LLPTE_GET_WRITE_PERMISSION_BIT(ll_pagetable_entry) == 0);
			KASSERT(LLPTE_GET_LOADED_BIT(ll_pagetable_entry) == 0);
			
			// 1. set dirty bit in TLB
			// spl = splhigh();
			int idx = tlb_probe(faultaddress, 0);
			if (idx > 0)
			{ 
				tlb_read(&entryhi, &entrylo, idx);
				entrylo |= (0b1 << 10); // set dirty bit
				tlb_write(entryhi, entrylo, idx);
			} 
			else
			{
				entryhi = TLPTE_MASK_VADDR(faultaddress); // kseg0 virtual address of the low level page table
				entrylo = (LLPTE_MASK_TLBE(ll_pagetable_entry)); // entries in the low level page table are aligned with the tlb
				entrylo |= (0b1 << 10); // set dirty bit
				tlb_random(entryhi, entrylo); // overwrite random entry
			}
			// splx(spl);

			// 2. set loaded bit in low level page table
			ll_pagetable_va[vpn2] |= (0b1 << 5); // set bit 5 (loaded) to 1

		break;

		case VM_FAULT_READ:
			// assert we are a user 
			// assert this page is indeed not in the TLB
			// spl = splhigh();
			KASSERT(curproc->p_addrspace != NULL); // TODO: Better way to check this?
			KASSERT(tlb_probe(faultaddress, 0) < 0);

			if(!LLPTE_GET_READ_PERMISSION_BIT(ll_pagetable_entry))
			{
				return EFAULT; // tried to read from a non readable page
			}

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

			if(!LLPTE_GET_WRITE_PERMISSION_BIT(ll_pagetable_entry) && LLPTE_GET_LOADED_BIT(ll_pagetable_entry))
			{
				return EFAULT;
			}
			// add to tlb
			entryhi = TLPTE_MASK_VADDR(faultaddress); // kseg0 virtual address of the low level page table
			entrylo = (LLPTE_MASK_TLBE(ll_pagetable_va[vpn2])); // entries in the low level page table are aligned with the tlb
			tlb_random(entryhi, entrylo); // Just randomly evict for now
			// splx(spl);

		break;

		splx(spl);
	}

	
	// why do we need to set the valid bit here?
	// why not set valid bit into PTE when we allocate the page?
	// so that when loaded into TLB, it is already valid
	// and then, we can clear the valid bit when we move to swap
	// which will also do TLB shootdown on the entry.
	ll_pagetable_va[vpn2] |= (0b1 << 9); // set bit 9 (valid bit) to 1

	// now we know that there is an actual translation in the page tables
	// uint32_t entrylo = (LLPTE_MASK_TLBE(ll_pagetable_va[vpn2])); // entries in the low level page table are aligned with the tlb
	// uint32_t entryhigh = faultaddress;
	
	// spl = splhigh();
	// tlb_random(entryhigh, entrylo); // Just randomly evict for now
	// splx(spl);

	return 0;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}

	as->heap_lk = lock_create("heap lock");
	if (as->heap_lk == NULL)
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
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
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
	result = alloc_upages(as, &va, npages, readable >> 2, writeable >> 1, executable);
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

	/* 
	 * as_create already creates the first page for for the top level table itself.
	 */
	struct addrspace *new = as_create();
	if (new == NULL)
	{
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
			memcpy(new_as_llpt, old_as_llpt, PAGE_SIZE);
			new->ptbase[i] = (vaddr_t)new_as_llpt | TLPTE_MASK_PAGE_COUNT((vaddr_t)old->ptbase[i]); // put in top-level pagetable
			
			
			for (int j = 0; j < 1024; j++)
			{
				if (old_as_llpt[j] != 0)
				{
					// allocate page, copy data into it, and update llpte
					vaddr_t* old_as_datapage = (vaddr_t *) PADDR_TO_KSEG0_VADDR(LLPTE_MASK_PPN(old_as_llpt[j])); // only PPN
					vaddr_t* new_as_datapage = (vaddr_t *) alloc_kpages(1);
					new->n_kuseg_pages_allocated++;
					memcpy(new_as_datapage, old_as_datapage, PAGE_SIZE);	
					new_as_llpt[j] = (vaddr_t)new_as_datapage | (old_as_llpt[j] & 0x00000f00); // NVDG flags
				}	
			}
		}
	}

	// At this point both must be equal or we did something wrong
	KASSERT(new->n_kuseg_pages_allocated == old->n_kuseg_pages_allocated); 
	
		
	*ret = new;
	return 0;
}



void 
free_upages(vaddr_t vaddr)
{
	int i;


	/**
	 * Free each page by getting each paddr from vaddr to vaddr + nppages * PAGE_SIZE
	 * and then bitmap free each page
	 * get lastpage bool from llpte
	 */

	while(1)
	{
		vaddr += (i * PAGE_SIZE);
		paddr_t paddr = translate_vaddr(vaddr);
		vaddr_t llpte = get_lltpe(vaddr);

		free_kpages(PADDR_TO_KSEG0_VADDR(paddr));

		if (LLPTE_GET_LASTPAGE_BIT(llpte))
		{
			// last page. exit
			break;
		}
		i++;
	}

	// TODO: we need to remove the pagetable entries
}

		
	
paddr_t
translate_vaddr(vaddr_t vaddr)
{
	int vpn1 = VADDR_GET_VPN1(vaddr);
	int vpn2 = VADDR_GET_VPN2(vaddr);

	struct addrspace *as = proc_getas();
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

vaddr_t get_lltpe(vaddr_t vaddr)
{
	int vpn1 = VADDR_GET_VPN1(vaddr);
	int vpn2 = VADDR_GET_VPN2(vaddr);

	struct addrspace *as = proc_getas();
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