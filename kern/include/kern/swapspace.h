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

#ifndef _SWAPSPACE_H_
#define _SWAPSPACE_H_

/**
 * @brief bootstrap the swap space system
 * 
 * @warning this function does not return as the swap space is not required for the system.
 * a warning will be given that bootstrap failed and the reason for it. 
 */
void 
swap_space_bootstrap(void);

/**
 * @brief reads one page from the swap space into a buffer
 * 
 * @param as address space making the call to the swap space
 * @param swap_location the offset of the data in swap space, needs to be page aligned
 * @param buf buffer to move the data read from the swap space into, must be 1 page in size
 * 
 * @return 0 on success, aligned with errno
 */
int 
read_from_swap(struct addrspace* as, int swap_idx, void * buf);

/**
 * @brief writes one page from the swap space into a buffer
 * 
 * @param as address space making the call to the swap space
 * @param swap_location the offset of the data in swap space, needs to be page aligned
 * @param buf buffer containing the data to be written into the swap space, must be 1 page in size
 * 
 * @return 0 on success, aligned with errno
 */
int 
write_page_to_swap(struct addrspace* as, int swap_idx, void* stolen_ppn);

vaddr_t
find_swapable_page(struct addrspace* as, bool* did_find);

/**
 * @brief allocates a single swap space page.
 * 
 * @return on success returns the index of the page aligned location in swap of the allocated page.
 * location 1 is at offset PAGE_SIZE, 2 is a 2*PAGE_SIZE, etc
 */
int
alloc_swap_page(void);

/**
 * @brief frees a single swap space
 * 
 * @param llpte the llpte that holds the offset index in the swap space
 * 
 */
void 
free_swap_page(paddr_t llpte);

/**
 * @brief frees a single swap space
 * 
 * @param as usaully the current address space for which you require a ram page
 * @param llpt the llpt of the page that is in swap space
 * @param vpn2 the index into the llpt 
 * 
 * @return returns the old virtual address of the page that was taken from ram
 * 
 */
paddr_t
swap_page(struct addrspace* as, vaddr_t* llpt, int vpn2);

#endif
