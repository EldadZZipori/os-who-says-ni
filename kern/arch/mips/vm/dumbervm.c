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


/* General VM stuff */

struct vm dumbervm;
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

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	// as = proc_getas();
	// if (as == NULL) {
	// 	/*
	// 	 * No address space set up. This is probably also a
	// 	 * kernel fault early in boot.
	// 	 */
	// 	return EFAULT;
	// }
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

	as->kseg2_fl_lk = lock_create("kseg2 lock");
	if (as->kseg2_fl_lk == NULL)
	{
		kfree(as);
		lock_destroy(as->heap_lk);
		return NULL;
	}

	as->kseg2_freelist = freelist_create((void*) MIPS_KSEG2, (void*) MIPS_KSEG2_END);
	as->user_heap_start = 0;
	as->user_heap_end = 0;
	as->asid = -1; // Not in use in TLB yet

	as->ptbase = getppages(1);	// Allocate physical page for the top level page table.
	
	as->n_kuseg2_pages_allocated = 1;
	as->n_kuseg_pages_allocated = 0;

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
	// size_t npages;

	// /* Align the region. First, the base... */
	// sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	// vaddr &= PAGE_FRAME;

	// /* ...and now the length. */
	// sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	// npages = sz / PAGE_SIZE;

	// /* We don't use these - all pages are read-write */
	// (void)readable;
	// (void)writeable;
	// (void)executable;

	// if (as->as_vbase1 == 0) {
	// 	as->as_vbase1 = vaddr;
	// 	as->as_npages1 = npages;
	// 	return 0;
	// }

	// if (as->as_vbase2 == 0) {
	// 	as->as_vbase2 = vaddr;
	// 	as->as_npages2 = npages;
	// 	return 0;
	// }

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return ENOSYS;
}

// static
// void
// as_zero_region(paddr_t paddr, unsigned npages)
// {
// 	bzero((void *)PADDR_TO_KSEG0_VADDR(paddr), npages * PAGE_SIZE);
// }

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
	(void) as;
	(void) stackptr;
	//KASSERT(as->as_stackpbase != 0);

	//*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	(void) old;
	(void) ret;
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
