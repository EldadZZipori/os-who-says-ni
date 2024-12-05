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

void
swap_space_bootstrap(void)
{
	struct vnode* swap_space;
	char swap_space_path[9] = "lhd0raw:";
	int result = vfs_open(swap_space_path, O_RDWR, 0, &swap_space);

	if (result)
	{
		kprintf("dumbervm: no swap space found, continuing without swap space");
		return;
	}

	dumbervm.swap_space = swap_space;

	dumbervm.swap_buffer = kmalloc(PAGE_SIZE);
	if (dumbervm.swap_buffer == NULL)
	{
		kprintf("dumbervm: can't create buffer for swap sapce");
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

	dumbervm.swap_bm = bitmap_create((swap_space_stat.st_size)/PAGE_SIZE);
	if (dumbervm.swap_bm == NULL)
	{
		vfs_close(swap_space);
		kprintf("dumbervm: cannot get swap space size, continuing without swap space");
		return;
	}

	dumbervm.swap_sz = swap_space_stat.st_size;
}

long long alloc_swap_page(void)
{
	unsigned int index;
	spinlock_acquire(&dumbervm.swap_bm_sl);
	int result = bitmap_alloc(dumbervm.swap_bm, &index);
	spinlock_release(&dumbervm.swap_bm_sl);


	if (!result)// this means there is space in the 
	{
		return index * 0x1000; // swap starts at 0 so should be simple
	}

	return ENOMEM;


}

void 
free_swap_page(paddr_t llpte)
{
	
	off_t swap_location = LLPTE_GET_SWAP_OFFSET(llpte);

	KASSERT(swap_location%PAGE_SIZE == 0);
	unsigned int index = swap_location;

	spinlock_acquire(&dumbervm.swap_bm_sl);
	bitmap_unmark(dumbervm.swap_bm, index);
	dumbervm.n_ppages_allocated--;
	spinlock_release(&dumbervm.swap_bm_sl);

}

vaddr_t
find_swapable_page(struct addrspace* as)
{
    if (as == NULL) {
        return 0;
    }

    int start_i = random() % 1024; 
    int start_j = random() % 1024;

    int i = start_i;
    do {
        if (as->ptbase[i] != 0) {
            vaddr_t* llpt = (vaddr_t*) TLPTE_MASK_VADDR(as->ptbase[i]);
            int j = start_j;
            do {
                if (llpt[j] != 0 && !LLPTE_GET_SWAP_BIT(llpt[j])) {
                    return (i << 22) | (j << 12);
                }
                j = (j + 1) % 1024;
            } while (j != start_j);
        }
        i = (i + 1) % 1024;
    } while (i != start_i);

    return 0; /* No swapable page found */
}

int 
write_stolen_page_to_swap(struct addrspace* as, off_t swap_location, paddr_t stolen_ppn)
{
	(void)as;
	struct uio uio;
    struct iovec iov;
	
	uio_kinit(&iov, &uio, (void *)PADDR_TO_KSEG0_VADDR(stolen_ppn), PAGE_SIZE, swap_location, UIO_WRITE);

	int result = VOP_WRITE(dumbervm.swap_space, &uio);
	return result;

}

int 
read_from_swap(struct addrspace* as, off_t swap_location, void * buf)
{
	(void)as;
	struct uio uio;
    struct iovec iov;
	uio_kinit(&iov, &uio, buf, PAGE_SIZE, swap_location, UIO_READ);

    // read from the file
    int result = VOP_READ(dumbervm.swap_space, &uio);

	return result;
	
}