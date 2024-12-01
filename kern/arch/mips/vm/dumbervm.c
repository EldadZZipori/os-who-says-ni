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
#include <kern/freelist.h>
#include <synch.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <vnode.h>


/* General VM stuff */

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;


void
vm_bootstrap(void)
{
	/*
	 * 1. Fetch bottom of ram
	 * 2. Fetch size of ram
	 * 3. Init a freelist to manage the physical space on our system
	 * 4. Add the freelist to the ram
	 */

	paddr_t ram_end = ram_getsize();
	paddr_t ram_start = ram_getfirstfree();

	// allocate freelist on stack
	struct freelist temp_ppage_fl; 
	struct freelist_node temp_ppage_fl_head; 

	temp_ppage_fl.start = (void*)ram_start;
	temp_ppage_fl.end = (void*)ram_end+1;
	temp_ppage_fl.head = &temp_ppage_fl_head;

	temp_ppage_fl.head->addr = (void*)ram_start;
	temp_ppage_fl.head->sz = ram_end - ram_start;
	temp_ppage_fl.head->next = NULL;
	temp_ppage_fl.head->prev = NULL;

	dumbervm.ppage_freelist = &temp_ppage_fl;
	dumbervm.vm_ready = true;


	// allocate first object in tracked RAM space: the RAM freelist lol
	struct freelist *ppage_freelist = freelist_create((void*)ram_start, (void*)ram_end+1);
	if (ppage_freelist == NULL) {
		panic("Can't allocate ppage freelist\n");
	}

	// make sure we didnt allocate any extra nodes
	KASSERT(temp_ppage_fl_head.next == NULL);

	// copy temp freelist to actual freelist
	freelist_copy(&temp_ppage_fl, ppage_freelist);

	// set the freelist to the actual freelist
	dumbervm.ppage_freelist = ppage_freelist;

	struct vnode* swap_space;
	int result = vfs_open("lhd0raw:",O_RDWR, 0, swap_space);

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

	struct freelist* swap_freelist = freelist_create((void *)0, (void*)swap_space_stat.st_size);
	if (swap_freelist == NULL)
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
	paddr_t pa;
	if (dumbervm.vm_ready)
	{
		// get first free physical page 
		pa = (paddr_t)freelist_get_first_fit(dumbervm.ppage_freelist, npages*PAGE_SIZE);
		freelist_node_set_otherpages(dumbervm.ppage_freelist, npages - 1);
		
		// NOTE: should be possible to return error here if out of memory

		KASSERT(pa >= (paddr_t)dumbervm.ppage_freelist->start);
		KASSERT(pa < (paddr_t)dumbervm.ppage_freelist->end);
	}
	else
	{
		spinlock_acquire(&stealmem_lock);

		pa = ram_stealmem(npages);

		spinlock_release(&stealmem_lock);
	}

	return pa;

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

	/* No freelist required */
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
	freelist_remove(dumbervm.ppage_freelist, (void*)paddr, PAGE_SIZE);
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
	(void) faulttype;
	(void) faultaddress;
	uint32_t ehi, elo;
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

	int vpn1 = TLPT_MASK(faultaddress);

	if(*(as->ptbase + vpn1) ==  0)	// top level page table was never created, no mapping
	{
		return EFAULT;
	}

	int vpn2 = LLPT_MASK(faultaddress);
	vaddr_t* ll_pagetable_va =  TLPT_ENTRY_TO_VADDR(*(as->ptbase + vpn1)); // the low level page table starts at the address stored in the top level page table entry

	if (*(ll_pagetable_va + vpn2) == 0) // there is no entry in the low level page table entry
	{
		return EFAULT;
	}

	// now we know that there is an actual translation in the page tables
	uint32_t entrylo = *(ll_pagetable_va + vpn2); // entries in the low level page table are aligned with the tlb
	uint32_t entryhigh = faultaddress;
	
	// uint32_t entryhi = VADDR_AND_ASIC_TO_TLB_HI(faultaddress, as->asid); ASID not used in os161
	spl = splhigh();

	for (int i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) { // if the valid bit is set in the entry, dont evict it
			continue;
		}
		//DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}

	kprintf("dumbvmer: Ran out of TLB entries - cannot handle page fault\n");
	splx(spl);
	return EFAULT;
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
	as->ptbase = alloc_kpages(1);	// Allocate physical page for the top level page table.
	as_zero_region(as->ptbase, 1); // Fill the top level page table with zeros
	as->n_kuseg_pages_allocated = 0;

	as->user_stackbase = MIPS_KSEG0;

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
	(void) as;
	(void) vaddr;
	(void)sz;
	(void) readable;
	(void) writeable;
	(void) executable;
	size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return ENOSYS;
}

static
void
as_zero_region(vaddr_t va, unsigned npages)
{
	bzero((void *)va , npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
	(void)as;
	//KASSERT(as->as_pbase1 == 0);
	//KASSERT(as->as_pbase2 == 0);
	//KASSERT(as->as_stackpbase == 0);

	// as->as_pbase1 = getppages(as->as_npages1);
	// if (as->as_pbase1 == 0) {
	// 	return ENOMEM;
	// }

	// as->as_pbase2 = getppages(as->as_npages2);
	// if (as->as_pbase2 == 0) {
	// 	return ENOMEM;
	// }

	// as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	// if (as->as_stackpbase == 0) {
	// 	return ENOMEM;
	// }

	// as_zero_region(as->as_pbase1, as->as_npages1);
	// as_zero_region(as->as_pbase2, as->as_npages2);
	// as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

	// return 0;

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

	*stackptr = as->user_stackbase;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	(void) old;
	(void) ret;

	/* 
	 * as_create already creates the first page for for the top level table itself.
	 */
	struct addrspace *new = as_create();
	if (new == NULL)
	{
		return ENOMEM; // This might not be the most idicative 
	}

	new->user_first_free_vaddr = old->user_first_free_vaddr;
	new->user_heap_start = old->user_heap_start;
	new->user_heap_end = old->user_heap_end;


	//memmove(new->ptbase, old->ptbase, PAGE_SIZE); // Copy over the high level page table
	int i = 0;
	for (i; i < PAGE_SIZE/sizeof(old->ptbase); i++)
	{
		if(*(old->ptbase + (i)) != 0)
		{
			vaddr_t new_ll_page_table_va = alloc_kpages(1);
			as_zero_region(new_ll_page_table_va, 1);
			*(new->ptbase + (i)) = new_ll_page_table_va; // put the virtual address of the low level page table into 

			vaddr_t* old_ll_page_table =  *(old->ptbase + (i));
			vaddr_t* new_ll_page_table = new_ll_page_table_va;
			int j = 0;

			for (j; j < 1024; j++)
			{
				if (*(old_ll_page_table + (j)) != 0)
				{
					vaddr_t copied_data_va = alloc_kpages(1);	// Allocate a page so we can move the data itself from old to new
					memmove(copied_data_va, PADDR_TO_KSEG0_VADDR(*(old_ll_page_table + (j))) , PAGE_SIZE); // move the data itself from the old page to the new page
					*(new_ll_page_table + (j)) = KSEG0_VADDR_TO_PADDR(copied_data_va); // put the physical address of the new data in the new ll page table entry
					// ll_pte = pt2[vpn2];
					j++;
				}	
			}
		}
	}

	new->n_kuseg_pages_allocated = old->n_kuseg_pages_allocated;
		
	*ret = new;
	return 0;

	// struct addrspace *new;

	// new = as_create();
	// if (new==NULL) {
	// 	return ENOMEM;
	// }

	// new->as_vbase1 = old->as_vbase1;
	// new->as_npages1 = old->as_npages1;
	// new->as_vbase2 = old->as_vbase2;
	// new->as_npages2 = old->as_npages2;

	// /* (Mis)use as_prepare_load to allocate some physical memory. */
	// if (as_prepare_load(new)) {
	// 	as_destroy(new);
	// 	return ENOMEM;
	// }

	// KASSERT(new->as_pbase1 != 0);
	// KASSERT(new->as_pbase2 != 0);
	// KASSERT(new->as_stackpbase != 0);

	// memmove((void *)PADDR_TO_KSEG0_VADDR(new->as_pbase1),
	// 	(const void *)PADDR_TO_KSEG0_VADDR(old->as_pbase1),
	// 	old->as_npages1*PAGE_SIZE);

	// memmove((void *)PADDR_TO_KSEG0_VADDR(new->as_pbase2),
	// 	(const void *)PADDR_TO_KSEG0_VADDR(old->as_pbase2),
	// 	old->as_npages2*PAGE_SIZE);

	// memmove((void *)PADDR_TO_KSEG0_VADDR(new->as_stackpbase),
	// 	(const void *)PADDR_TO_KSEG0_VADDR(old->as_stackpbase),
	// 	DUMBVM_STACKPAGES*PAGE_SIZE);

	// *ret = new;
	// return 0;
	
	return 0;
}


vaddr_t 
alloc_upages(unsigned npages)
{
	spinlock_acquire(&curproc->p_lock); // Need to make sure no one changes address space on us

    struct addrspace* as = curproc->p_addrspace;

	// Allocate memory here
	/*
	* our as_create allocates one page for the top level page table itself.
	*/

	// NOTE: Since we are allocating one block at a time we are not going to have a problem with low level page table getting full 
	// while we are allocating
	for (int i = 0; i < npages; i++)
	{
		int vpn1 = TLPT_MASK(as->user_first_free_vaddr);
    	int vpn2 = LLPT_MASK(as->user_first_free_vaddr);

		vaddr_t va = alloc_kpages(1);
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

		as->user_first_free_vaddr += 0x1000;
	}

	spinlock_release(&curproc->p_lock);
}

void 
free_upages(vaddr_t addr)
{
	// For Kyle cause I don't know how the size thing work in free_list
	freelist_remove(ppage_freelist, addr, PAGE_SIZE);

	/*
		pointer = malloc 
		free(pointer+1)
		free(pointer)

		alloc_upages(npages=3) {
			when alloc first save freelist_node.npages = 3
		}	

		
		
	*/
}
