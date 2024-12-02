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
#include <kern/memlist.h>
#include <synch.h>
#include <vfs.h>
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <vnode.h>


#define DUMBVMER_STACKPAGES    18


/* General VM stuff */

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;


void
vm_bootstrap(void)
{
	/*
	 * 1. Fetch bottom of ram
	 * 2. Fetch size of ram
	 * 3. Init a memlist to manage the physical space on our system
	 * 4. Add the memlist to the ram
	 */

	paddr_t ram_end = ram_getsize();
	paddr_t ram_start = ram_getfirstfree();

	// allocate memlist on stack
	struct memlist temp_ppage_fl; 
	struct memlist_node temp_ppage_fl_head; 

	temp_ppage_fl.start = ram_start;
	temp_ppage_fl.end = ram_end+1;
	temp_ppage_fl.head = &temp_ppage_fl_head;

	temp_ppage_fl.head->paddr = (paddr_t)ram_start;
	temp_ppage_fl.head->sz = ram_end - ram_start;
	temp_ppage_fl.head->next = NULL;
	temp_ppage_fl.head->prev = NULL;

	dumbervm.ppage_memlist = &temp_ppage_fl;
	dumbervm.vm_ready = true;


	// allocate first object in tracked RAM space: the RAM memlist lol
	struct memlist *ppage_memlist = memlist_create(ram_start, ram_end+1);
	if (ppage_memlist == NULL) {
		panic("Can't allocate ppage memlist\n");
	}

	// make sure we didnt allocate any extra nodes
	KASSERT(temp_ppage_fl_head.next == NULL);

	// copy temp memlist to actual memlist
	memlist_copy(&temp_ppage_fl, ppage_memlist);

	// set the memlist to the actual memlist
	dumbervm.ppage_memlist = ppage_memlist;

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

	struct memlist* swap_memlist = memlist_create((paddr_t)0, (paddr_t)(uintptr_t)swap_space_stat.st_size);
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
	bzero((void *)va , npages * PAGE_SIZE);
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
		pa = (paddr_t)memlist_get_first_fit(dumbervm.ppage_memlist, npages*PAGE_SIZE);

		// TODO: add the actuall node here
		struct memlist_node *n = memlist_get_node(dumbervm.ppage_memlist, pa);
		memlist_node_set_otherpages(n, 0);
		
		// NOTE: should be possible to return error here if out of memory

		KASSERT(pa >= (paddr_t)dumbervm.ppage_memlist->start);
		KASSERT(pa < (paddr_t)dumbervm.ppage_memlist->end);
	}
	else
	{
		spinlock_acquire(&stealmem_lock);

		pa = ram_stealmem(npages);

		spinlock_release(&stealmem_lock);
	}

	return pa;

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

		vaddr_t kseg0_va = alloc_kpages(1);

		// set the 'otherpages' field in the memlist node of the first page in the block
		if (i == 0) {
			struct memlist_node *n = memlist_get_node(dumbervm.ppage_memlist, KSEG0_VADDR_TO_PADDR(kseg0_va));
			memlist_node_set_otherpages(n, npages - 1);
		}

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

		ll_pagetable_va[vpn2] = pa & ((readable << 2) & (writeable << 1) & (executable)); // this will be page aligned

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
	if (stacktop == USERSPACETOP) {
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
	memlist_remove(dumbervm.ppage_memlist, paddr);
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
	
	switch (faulttype)
	{
		case VM_FAULT_READONLY:
			// we tried to write to a read only page that is already in the TLB
			// or we tried to write to a page whose dirty bit is not set yet
			if (LLPTE_GET_WRITE_PERMISSION_BIT(ll_pagetable_entry))
			{
				// set dirty bit
				ll_pagetable_va[vpn2] |= (0b1 << 10); // set bit 10 to 1
			} else {
				// hard fault
			}
		break;
		case VM_FAULT_READ:
			// we tried to read from a page not in the TLB
			// but is in pagetable.
			if(!LLPTE_GET_READ_PERMISSION_BIT(ll_pagetable_entry))
			{
				return EFAULT; // tried to read from a non readable page
			}
			// add to tlb 
			// and set valid bit
			
		break;
		case VM_FAULT_WRITE:
			// we tried to write to a page not in the TLB...? I think?
			// but is in pagetable.
			if(!LLPTE_GET_WRITE_PERMISSION_BIT(ll_pagetable_entry))
			{
				return EFAULT;
			}
			// add to tlb 
			// and set valid bit 
			// and then set dirty bit? or call vm_fault
		break;
	}
	ll_pagetable_va[vpn2] |= (0b1 << 9); // set bit 9 (valid bit) to 1
	// now we know that there is an actual translation in the page tables
	uint32_t entrylo = (LLPTE_MASK_TLBE(ll_pagetable_va[vpn2])); // entries in the low level page table are aligned with the tlb
	uint32_t entryhigh = faultaddress;
	
	spl = splhigh();
	tlb_random(entryhigh, entrylo); // Just randomly evict for now
	splx(spl);

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
	result = alloc_upages(as, &va, npages, readable, writeable, executable);
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

	new->user_heap_start = old->user_heap_start;
	new->user_heap_end = old->user_heap_end;
	int result;

	vaddr_t first_va = 0;
	result = alloc_upages(new , &first_va,(old->n_kuseg_pages_allocated - DUMBVMER_STACKPAGES), 0, 0,0);
	new->user_first_free_vaddr = first_va;
	as_zero_region(0, (old->n_kuseg_pages_allocated - DUMBVMER_STACKPAGES));
	result = as_create_stack(new);
	if(result)
	{
		as_destroy(new);
		return result;
	}

	// Copy the data only from the old address to the new address, the mapping are all ready the same
	// this will copy all the data over both from bottom of the userspace and from the top (stack)
	for (int i = 0; i < 1024; i++)
	{
		if (old->ptbase[i] != 0) 
		{
			vaddr_t *old_as_llpt = (vaddr_t *)old->ptbase[i];
			vaddr_t *new_as_llpt = (vaddr_t *)new->ptbase[i]; // we need to allocate this
			//vaddr_t *new_as_llpt = (vaddr_t *)alloc_upages(1); // will be page-aligned
			new->ptbase[i] = (vaddr_t)((int32_t)new_as_llpt | (int32_t)TLPTE_MASK_PAGE_COUNT((int32_t)(old->ptbase[i]))); // Add the llpte count into the tlpte 
		
			for (int j = 0; j < 1024; j++)
			{
				if (old_as_llpt[j] != 0)
				{
					paddr_t new_ppn = LLPTE_MASK_PPN(new_as_llpt[j]);
					paddr_t old_ppn = LLPTE_MASK_PPN(old_as_llpt[j]);
					memcpy((void * )PADDR_TO_KSEG0_VADDR(new_ppn), (void *)PADDR_TO_KSEG0_VADDR(old_ppn), PAGE_SIZE);
					new_as_llpt[j] = new_as_llpt[j] & LLPTE_MASK_NVDG_FLAGS(old_as_llpt[j]) & LLPTE_MASK_RWE_FLAGS(old_as_llpt[j]);
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
	int nppages;

	// TODO: not sure what we need to assert here about the vaddr
	KASSERT(true); // leave empty for now 

	// get the paddr through the page table 
	paddr_t paddr = translate_vaddr(vaddr);

	// get the page count from the memlist 
	struct memlist_node *n = memlist_get_node(dumbervm.ppage_memlist, paddr);
	nppages = memlist_node_get_otherpages(n) + 1;

	/**
	 * Free each page by getting each paddr from vaddr to vaddr + nppages * PAGE_SIZE
	 * and then memlist remove each page
	 */

	for (int i = 0; i < nppages; i++)
	{
		paddr_t paddr = translate_vaddr(vaddr + (i * PAGE_SIZE));
		memlist_remove(dumbervm.ppage_memlist, paddr);
	}

	// TODO: we need to remove the pagetable entries
}


// 	// For Kyle cause I don't know how the size thing work in free_list
// 	//memlist_remove(ppage_memlist, addr, PAGE_SIZE);

// 	/*
// 		pointer = malloc 
// 		free(pointer+1)
// 		free(pointer)

// 		alloc_upages(npages=3) {
// 			when alloc first save memlist_node.npages = 3
// 		}	

		
		
// 	*/
// }
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