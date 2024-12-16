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

	dumbervm.swap_buffer = (vaddr_t *) alloc_kpages(1,false);
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

	int swap_pages = (swap_space_stat.st_size)/PAGE_SIZE;

	dumbervm.swap_bm = bitmap_create(swap_pages);
	if (dumbervm.swap_bm == NULL)
	{
		vfs_close(swap_space);
		kprintf("dumbervm: cannot get swap space size, continuing without swap space");
		return;
	}

	dumbervm.swap_sz = swap_space_stat.st_size;

	dumbervm.fault_lk = lock_create("fault lock");
	if (dumbervm.fault_lk == NULL)
	{
		panic("dummervm: can't really survive without the fault lock and swap");
	}
	dumbervm.kern_lk = lock_create("kern lock");
	if (dumbervm.kern_lk == NULL)
	{
		panic("dumbervm: can't survive without a kern lock and swap space");
	}
}

int 
alloc_swap_page(void)
{
	// TODO: this is a bad return. we need to be able to find errors
	unsigned int index;
	//spinlock_acquire(&dumbervm.swap_bm_sl);
	int result = bitmap_alloc(dumbervm.swap_bm, &index);
	//spinlock_release(&dumbervm.swap_bm_sl);


	if (!result)// this means there is space in the 
	{
		return index; // swap starts at 0 so should be simple
	}

	return -1;


}

void 
free_swap_page(paddr_t llpte)
{
	unsigned int index = LLPTE_GET_SWAP_OFFSET(llpte);	
	off_t swap_location = index * PAGE_SIZE;

	KASSERT(swap_location%PAGE_SIZE == 0 || swap_location == 0);
	

	//spinlock_acquire(&dumbervm.swap_bm_sl);
	bitmap_unmark(dumbervm.swap_bm, index);
	//spinlock_release(&dumbervm.swap_bm_sl);

	zero_swap_page(index);
}

paddr_t
replace_ram_page_with_swap_page(struct addrspace* as, vaddr_t* llpt, int vpn2)
{
	(void)llpt; 
	(void)vpn2; 

	int swap_idx = LLPTE_GET_SWAP_OFFSET(llpt[vpn2]);
	bool did_find = true;
	vaddr_t ram_page_vaddr = find_swapable_page(as, &did_find, false); // find a page that belongs to the user so we can steal it
	struct tlbshootdown ts;
	ts.va = ram_page_vaddr; 
	//ipi_tlbshootdown_all(&ts);
	vm_tlbshootdown(&ts);
	
	if (!did_find)
	{
		panic("\n7\n");
		return ENOMEM; // wasnt enough pages that we can swap.
	}
	int ram_page_vpn1 = VADDR_GET_VPN1(ram_page_vaddr);
	int ram_page_vpn2 = VADDR_GET_VPN2(ram_page_vaddr);
	
	vaddr_t* ram_page_llpt = (vaddr_t *)TLPTE_MASK_VADDR(as->ptbase[ram_page_vpn1]); // get the original llpte
	paddr_t ram_page = ram_page_llpt[ram_page_vpn2];  // get the physical address of the page we are going to use to store our data
	paddr_t ram_ppn = LLPTE_MASK_PPN(ram_page); 

	ram_page_llpt[ram_page_vpn2] = LLPTE_SET_SWAP_BIT(swap_idx << 12); // mark that we are putting this data in the swap space

	int result = read_from_swap(as, swap_idx, dumbervm.swap_buffer); // copy data from swap to a buffer before writing the stolen data to it
	if (result)
	{
		
		kprintf("dumbervm: problem reading from swap space\n");
		return ENOMEM;
	}

	result = write_page_to_swap(as, swap_idx, (void *)PADDR_TO_KSEG0_VADDR(ram_ppn)); // save the stolen data into the swap space

	if (result)
	{
		kprintf("dumbervm: problem writing to swap space\n");
		return ENOMEM;
	}

	memcpy((void *)PADDR_TO_KSEG0_VADDR(ram_ppn), dumbervm.swap_buffer, PAGE_SIZE); // copy the data that was in swap into the ppn we just stole

	as_zero_region((vaddr_t)dumbervm.swap_buffer, 1);

	llpt[vpn2] = (ram_ppn) | TLBLO_DIRTY | TLBLO_VALID ;//mark the stolen ppn on the translation for the fault virtual address

	return ram_page_vaddr;
}


int 
zero_swap_page(int swap_idx)
{
	struct uio uio;
    struct iovec iov;

	off_t offset_in_swap = swap_idx * PAGE_SIZE;

	memset(dumbervm.swap_buffer, 0, PAGE_SIZE);

	uio_kinit(&iov, &uio, (void *)(dumbervm.swap_buffer), PAGE_SIZE, offset_in_swap, UIO_WRITE);

	int result = VOP_WRITE(dumbervm.swap_space, &uio);
	return result;
	
}

vaddr_t
find_swapable_page(struct addrspace* as, bool* did_find, bool can_be_exec)
{
    if (as == NULL) {
        return 0;
    }

    int start_i = 0;//random() % 1024; 
    int start_j = 0;//random() % 1024;

	bool is_executable = false;

    int i = start_i;
    do {
        if (as->ptbase[i] != 0) {
			if (!TLPTE_GET_SWAP_BIT(as->ptbase[i]))
			{
				vaddr_t* llpt = (vaddr_t*) TLPTE_MASK_VADDR(as->ptbase[i]);
				int j = start_j;
				do {
					is_executable =  LLPTE_GET_EXECUTABLE(llpt[j]);
					/* 
					* If is_executable & can_be_executable - can be returned - 1
					* If is_executable & !can_be_executable - cannot returned - 0
					* If !is_executable & can_be_executable - can return - 1
					* If !is _executable & !can_be_executable  - can return - 1
					*/
					if (llpt[j] != 0 && !LLPTE_GET_SWAP_BIT(llpt[j])) {
						if (!(is_executable && (can_be_exec == false)))
						{
							*did_find = true;
							return (i << 22) | (j << 12);
						}
					}
					j = (j + 1) ;//% 1024;
				} while (j != 1024);
			}
        }
        i = (i + 1) ;//% 1024;
    } while (i != 1024);

	*did_find = false;
    return 0; /* No swapable page found */
}


int 
write_page_to_swap(struct addrspace* as, int swap_idx, void *buf)
{
	(void)as;
	struct uio uio;
    struct iovec iov;
	
	off_t swap_offset = swap_idx * PAGE_SIZE;

	uio_kinit(&iov, &uio, buf, PAGE_SIZE, swap_offset, UIO_WRITE);

	int result = VOP_WRITE(dumbervm.swap_space, &uio);
	return result;
}

int 
read_from_swap(struct addrspace* as, int swap_idx, void * buf)
{
	(void)as;
	struct uio uio;
    struct iovec iov;

	off_t swap_offset = swap_idx * PAGE_SIZE;
	uio_kinit(&iov, &uio, buf, PAGE_SIZE, swap_offset, UIO_READ);

    // read from the file
    int result = VOP_READ(dumbervm.swap_space, &uio);

	return result;
	
}