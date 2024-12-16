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
    struct lock* exec_lk;

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
 * @return the kseg0 virtual address of the first page allocated
 * 
 * Marks the physical page bitmap as well as the last-page bitmap, which is 
 * used to determine the size of the allocated data when we want to free it.
 */
vaddr_t 
alloc_kpages(unsigned npages, bool kmalloc);

/**
 * @brief Allocates a page for the kernel. This allocates continuous pages. 
 * 
 * @param npages number of pages to be allocated
 * 
 * @return the virtual address (KSEG0) of the first allocated page.
 * 
 * Uses the previously-marked last-page bitmap; when freeing a page, 
 * if the bit for this pag in the last-page bitmap is not set, that 
 * means the next physical page is part of this same allocation and 
 * should be freed. Iterate through physical RAM until we hit a page 
 * whose last-page bit is set. 
 */
void 
free_kpages(vaddr_t addr, bool is_kfree);

/**
 * @brief Allocates pages for the program heap
 * 
 * @param as address space
 * @param npages number of pages to be allocated
 * 
 * @return 0 on success, error on failure
 */
int 
alloc_heap_upages(struct addrspace* as, int npages);

/**
 * @brief frees user heap pages
 * 
 * @param as address space
 * @param npages number of pages to be freed
 * 
 * @return 0 on success, error on failure
 * 
 * Under the hood, relies on free_kpages to clear the necessary bits 
 * that represent the physical pages in the physical page bitmap and 
 * the last-page bitmap. 
 */
int 
free_heap_upages(struct addrspace* as, int npages);

/**
 * @brief Allocates pages for the program heap
 * 
 * @param as address space
 * @param va starting virtual address to allocate
 * @param in_swap return value to indicate if it was put in swap.
 * @param readable read access for this allocation
 * @param writeable write access for this allocation
 * @param executable execution access for this allocation
 * 
 * @return 0 on success, error on failure
 */
int
alloc_upages(struct addrspace* as, vaddr_t* va, unsigned npages, bool* in_swap,int readable, int writeable, int executable);

/** 
 * @brief frees user pages
 * 
 * @param as address space
 * @param npages number of pages to be freed
 * 
 * Also cleans up the pagetables when needed. 
 * 
 * Uses on free_kpages to clean up the physical page and last-page bitmaps. 
 */
void 
free_upages(struct addrspace* as, vaddr_t vaddr);

/** 
 * @brief find the physical ram location of a user space virtual address
 * 
 * @param as address space the translation belongs to
 * @param vaddr the user virtual address to translate
 * 
 * @return the PPN of that this virtual address translates to
 */
paddr_t 
translate_vaddr_to_paddr(struct addrspace* as, vaddr_t vaddr);


/**
 * @brief finds the translation for a given user virtual address ,i.e. the low level page table entry
 * 
 * @param as address space the translation belongs to
 * @param vaddr the user virtual address to translate
 * 
 * @return the entry for the translation of the user virtual address, contains both PPN and some bit flags
 */
vaddr_t 
get_lltpe(struct addrspace* as, vaddr_t vaddr);

/* 
 * TLB shootdown handling called from interprocessor_interrupt 
 */

/**
 * @brief Invalidates the whole tlb on the current CPU
 * 
 */ 
void 
vm_tlbshootdown_all(void);

/**
 * @brief Shoots down one translationin th tlb
 * 
 */
void 
vm_tlbshootdown(const struct tlbshootdown *);

/**
 * @brief Invalidates the whole tlb on the current CPU
 * 
 */
void 
invalidate_tlb(void);



#endif /* _VM_H_ */
