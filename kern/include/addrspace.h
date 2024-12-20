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

#ifndef _ADDRSPACE_H_
#define _ADDRSPACE_H_

/*
 * Address space structure and operations.
 */


#include <vm.h>
#include "opt-dumbvm.h"

#define MIPS_KSEG2_END       0xffffffff
#define MIPS_KUSEG_END       0x7fffffff


/* VADDR MACROS */
#define VADDR_GET_OFFSET(x)  ((x) & 0xfff)       // First 12 bits of address
#define VADDR_GET_VPN1(x)    (((x)>>22) & 0x3ff) // Bit 22-31
#define VADDR_GET_VPN2(x)    (((x)>>12) & 0x3ff) // Bit 12-21

/* LLPTE MACROS */
#define LLPTE_GET_READ_PERMISSION_BIT(x)        (((x)>>2) & 0b1)
#define LLPTE_GET_WRITE_PERMISSION_BIT(x)       (((x)>>1) & 0b1)
#define LLPTE_GET_DIRTY_BIT(x)                  (((x)>>10) & 0b1)
#define LLPTE_GET_LASTPAGE_BIT(x)               (((x)>>4) & 0b1)
#define LLPTE_GET_LOADED_BIT(x)                 (((x)>>5) & 0b1)
#define LLPTE_GET_EXECUTABLE(x)                 ((x) & 0b1)
#define LLPTE_MASK_PPN(x)                       ((x) & 0xfffff000)
#define LLPTE_MASK_TLBE(x)                      ((x) & 0xffffff00)
#define LLPTE_MASK_NVDG_FLAGS(x)                ((x) & 0x00000f00)
#define LLPTE_MASK_RWE_FLAGS(x)                 ((x) & 0x7)
#define LLPTE_SET_SWAP_BIT(x)                   ((x) | 0b1000)
//#define LLPTE_UNSET_SWAP_BIT(x)                 ((x) & 0b0111)
#define LLPTE_GET_SWAP_BIT(x)                   ((x>>3) & 0b1)
#define LLPTE_GET_SWAP_OFFSET(x)                ((x>>12) & 0xfffff)

/* TLPTE MACROS */
#define TLPTE_MASK_SWAP_BIT(x)                ((x) & 0x00000001)
#define TLPTE_MASK_VADDR(x)                     ((x) & 0xfffff000)
#define TLPTE_GET_SWAP_BIT(x)                    ((x) &0b1)
#define TLPTE_GET_SWAP_IDX(x)                    ((x>>12) & 0xfffff)


struct vnode;

/*
 * Address space - data structure associated with the virtual memory
 * space of a process.
 *
 * You write this.
 */

struct addrspace {
#if OPT_DUMBVM
        vaddr_t as_vbase1;              // Virtual address base
        paddr_t as_pbase1;              // Physical address base
        size_t as_npages1;              // we know page size = 4096, so this gives size of mem
        vaddr_t as_vbase2;              // why do we have two of these?
        paddr_t as_pbase2;
        size_t as_npages2;
        paddr_t as_stackpbase;          // the physical base of the stack
#else
        /* Put stuff here for your VM system */
        // uint8_t asid; not necessary 
        vaddr_t* ptbase;

        /* KUSEG */ 
        vaddr_t user_heap_start;
        vaddr_t user_heap_end;

        vaddr_t user_first_free_vaddr;
        int n_kuseg_pages_swap;
        int n_kuseg_pages_ram;

        /* User stack */
        vaddr_t user_stackbase; // User stack is part of KUSEG so it is translated in the tlb

#endif
};

/*
 * Functions in addrspace.c:
 *
 *    as_create - create a new empty address space. You need to make
 *                sure this gets called in all the right places. You
 *                may find you want to change the argument list. May
 *                return NULL on out-of-memory error.
 *
 *    as_copy   - create a new address space that is an exact copy of
 *                an old one. Probably calls as_create to get a new
 *                empty address space and fill it in, but that's up to
 *                you.
 *
 *    as_activate - make curproc's address space the one currently
 *                "seen" by the processor.
 *
 *    as_deactivate - unload curproc's address space so it isn't
 *                currently "seen" by the processor. This is used to
 *                avoid potentially "seeing" it while it's being
 *                destroyed.
 *
 *    as_destroy - dispose of an address space. You may need to change
 *                the way this works if implementing user-level threads.
 *
 *    as_define_region - set up a region of memory within the address
 *                space.
 *
 *    as_prepare_load - this is called before actually loading from an
 *                executable into the address space.
 *
 *    as_complete_load - this is called when loading from an executable
 *                is complete.
 *
 *    as_define_stack - set up the stack region in the address space.
 *                (Normally called *after* as_complete_load().) Hands
 *                back the initial stack pointer for the new process.
 *
 * Note that when using dumbvm, addrspace.c is not used and these
 * functions are found in dumbvm.c.
 */

struct addrspace *as_create(void);
int               as_copy(struct addrspace *src, struct addrspace **ret);
void              as_activate(bool invalidate);
void              as_deactivate(void);
void              as_destroy(struct addrspace *);

int               as_define_region(struct addrspace *as,
                                   vaddr_t vaddr, size_t sz,
                                   int readable,
                                   int writeable,
                                   int executable);
int               as_prepare_load(struct addrspace *as);
int               as_complete_load(struct addrspace *as);
int               as_define_stack(struct addrspace *as, vaddr_t *initstackptr);

/*
 * Functions in loadelf.c
 *    load_elf - load an ELF user program executable into the current
 *               address space. Returns the entry point (initial PC)
 *               in the space pointed to by ENTRYPOINT.
 */
int               load_elf(struct vnode *v, vaddr_t *entrypoint);



/*
 * Additional Code for Virtual Machine implementation
 */

void 
as_zero_region(vaddr_t va, unsigned npages);


/** 
 * @brief allocates stack pages for a users stack
 * 
 * @param as address space to allocate stack region for
 * 
 * @return 0 on success, error otherwise
 * 
 **/
int 
as_create_stack(struct addrspace* as);


/** 
 * @brief moves ram pages from an addresspace to swap
 * 
 * @param npages_to_swap max number to move swap
 * @param num_pages_swapped number of pages that were actually moved to swap
 * 
 * @return 0 on success, -1 on failirue
 * 
 **/
int
as_move_to_swap(struct addrspace* as, int npages_to_swap, int *num_pages_swapped);


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
as_load_pagetable_from_swap(struct addrspace *as, int swap_idx, int vpn1);

/**
 * @brief move a lower-level page table to swap space
 * 
 * @param llpt lower-level page table (ptbase[i] suffices)
 * 
 * Replaces the llpt address (kseg0 vaddr) with the swap space index
 */

int
as_move_pagetable_to_swap(struct addrspace* as, int vpn1);
#endif /* _ADDRSPACE_H_ */
