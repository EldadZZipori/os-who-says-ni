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

#ifndef _VM_H_
#define _VM_H_

/*
 * VM system-related definitions.
 *
 * You'll probably want to add stuff here.
 */


#include <machine/vm.h>
#include <kern/types.h>
#include <addrspace.h>
#include <spinlock.h>

/*
 * Struct for managing the background opperation of the virtual machine.
 */
struct vm
{
    struct bitmap *ppage_bm;
    struct bitmap *ppage_lastpage_bm;

    struct bitmap *swap_bm; // Holds offset 0- size of swap space

    unsigned int n_ppages;
    unsigned int n_ppages_allocated;

    struct vnode *swap_space;

    void* swap_buffer;

    paddr_t ram_start;

    struct lock* fault_lk;
    struct lock* kern_lk;

    //struct spinlock ppage_bm_sl;
   // struct spinlock swap_bm_sl;

    long swap_sz;

    bool vm_ready;
};

struct vm dumbervm;

/* Fault-type arguments to vm_fault() */
#define VM_FAULT_READ        0    /* A read was attempted */
#define VM_FAULT_WRITE       1    /* A write was attempted */
#define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

/* Static constants for address spaces */
#define DUMBVMER_STACKPAGES    8
#define DUMB_HEAP_START	0x1000000

/**
 * @brief bootstrap the virtual machine of the system
 * 
 * @warning this function does not return. If this cannot be done the system will panic.
 */
void 
vm_bootstrap(void);


/* Fault handling function called by trap code */
int 
vm_fault(int faulttype, vaddr_t faultaddress);

/**
 * @brief Allocates a page for the kernel. This allocates continuous pages. 
 * 
 * @param npages number of pages to be allocated
 * 
 * @return the virtual address (KSEG0) of the first allocated page.
 */
vaddr_t 
alloc_kpages(unsigned npages, bool kmalloc);

/**
 * @brief Allocates a page for the kernel. This allocates continuous pages. 
 * 
 * @param npages number of pages to be allocated
 * 
 * @return the virtual address (KSEG0) of the first allocated page.
 */
void 
free_kpages(vaddr_t addr, bool is_kfree);

int 
alloc_heap_upages(struct addrspace* as, int npages);

int 
free_heap_upages(struct addrspace* as, int npages);

int
alloc_upages(struct addrspace* as, vaddr_t* va, unsigned npages, bool* in_swap,int readable, int writeable, int executable);

void 
free_upages(struct addrspace* as, vaddr_t vaddr);

paddr_t 
translate_vaddr_to_paddr(struct addrspace* as, vaddr_t vaddr);


vaddr_t 
get_lltpe(struct addrspace* as,vaddr_t vaddr);

/* TLB shootdown handling called from interprocessor_interrupt */
void 
vm_tlbshootdown_all(void);


void 
vm_tlbshootdown(const struct tlbshootdown *);


void 
invalidate_tlb(void);

int
as_move_pagetable_to_swap(struct addrspace* as, int vpn1);

int
as_load_pagetable_from_swap(struct addrspace *as, int swap_idx, int vpn1);


#endif /* _VM_H_ */
