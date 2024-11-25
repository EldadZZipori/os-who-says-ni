#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <freelist.h>


/**
 * @brief create a freelist for an empty virtual or physical memory region
 * 
 * @param start void pointer, representing a physical or virtual address,
 *              of the start of the memory region (inclusive)
 * @param end void pointer, representing a physical or virtual address, 
 *              of the end of hte memory region (exclusive)
*/
struct freelist* freelist_create(void *start, void *end) {

    struct freelist *fl = kmalloc(sizeof(struct freelist));
    if (fl == NULL) 
    {
        return NULL;
    }

    fl->head = kmalloc(sizeof(struct freelist_node));
    if (fl->head == NULL) 
    {
        kfree(fl);
        return NULL;
    }

    fl->fl_lk = lock_create("freelist lock");
    if (fl->fl_lk == NULL)
    {
        kfree(fl->head);
        kfree(fl);
        return NULL;
    }

    fl->start = start; 
    fl->end = end;
    fl->head->addr = start;
    fl->head->size = end - start;
    fl->head->next = NULL;
    fl->head->prev = NULL;

    return fl;
}

void freelist_destroy(struct freelist *fl) {
    KASSERT(fl != NULL);

    struct freelist_node *cur = fl->head->next;

    while (cur != NULL) 
    {
        kfree(cur->prev);
        cur = cur->next;
    }
    
    lock_destroy(fl->fl_lk);
    kfree(fl);
}

/** 
 * @brief gets a free block and updates the freelist accordingly 
 * 
 * @param fl freelist 
 * @param size size of the block to allocate
*/
vaddr_t freelist_get_first_fit(struct freelist *fl, size_t sz) {
    KASSERT(fl != NULL);
    KASSERT(sz > 0);
    /** 
     * What we need to do: 
     * 1. Find the first block that fits the size
     * 2. If block size is equal to size, remove the block from the freelist
     *    and update nodes 
     * 3. If block size is greater than size, adjust pointers and sizes
     * 
     * 
     * 
     * Case 1: Find block equal to size 
     * e.g.: get_first_fit(100) 
     * 
     * Before: 
     * fl -> 40 -> 60 -> 100 -> 200 -> NULL
     * 
     * After: 
     *             100B allocated 
     *                 v 
     * fl -> 40 -> 60 -> 200 -> NULL
     * 
     * 
     * 
     * Case 2: Find block larger than size
     * e.g. get_first_fit(70) 
     * 
     * Before: 
     * fl -> 40 -> 60 -> 100 -> 200 -> NULL
     * 
     * After: 
     *              70B allocated
     *                  v                   
     * fl -> 40 -> 60 -> 30 -> 200 -> NULL
    */
    struct freelist_node *cur = fl->head;
    while (cur != NULL) 
    {
        // remove the block from the freelist
        if (cur->size == sz)
        { 
            // case 1: head is block to remove 
            if (cur->prev == NULL) 
            {
                fl->head = fl->head->next;
            } 
            else 
            // case 2: not removing head
            {
                cur->prev->next = cur->next;
            }
            return cur->addr; 
        }
        elif (cur->size > sz) 
        {
            // allocate at start of block
            cur->addr += sz;
            cur->sz -= sz;
            return cur->addr - sz;
        }
    } 
    return NULL;
}

void freelist_remove(struct freelist *fl, void *blk, size_t sz)
{
    KASSERT(fl != NULL);
    KASSERT(fl->head != NULL);
    KASSERT(blk < fl->end);
    KASSERT(blk >= fl->start);

    // find the location of the allocated block to free 
    struct freelist_node *cur = fl->head; 
    struct freelist_node *new; 

    new->addr = blk; 
    new->size = sz; 

    // Case 1: if freelist is empty, add node
    if (cur == NULL) {
        fl->head = new;
        new->next = NULL;
        new->prev = NULL;
        return;
    }

   // Insert before 'cur' if not NULL, otherwise insert at end of list
    if (cur != NULL) {
        // Insert the new node before 'cur'
        new->next = cur;
        new->prev = cur->prev;

        if (cur->prev != NULL) {
            cur->prev->next = new; // update ptr before new node 
        } else {
            fl->head = new; // insert at head of list  
        }

        cur->prev = new;
    } else {
        // we're at end of list
        new->next = NULL;
        new->prev = cur;
        cur->next = new;
    }

    // now try to merge adjacent free blocks
    // merge with prev block if possible
    if (new->prev != NULL && new->prev->addr + new->prev->size == new->addr) {
        new->prev->size += new->size; // merge sizes
        new->prev->next = new->next;  // remove 'new' from list

        if (new->next != NULL) {
            new->next->prev = new->prev;
        }

        new = new->prev;
    }

    // merge with next block if possible
    if (new->next != NULL && new->addr + new->size == new->next->addr) {
        new->size += new->next->size; // merge sizes
        struct freelist_node *temp = new->next;
        new->next = new->next->next; // remove intermediate node forward ptr
        if (new->next != NULL) {
            new->next->prev = new; // remove intermediate notde backward ptr
        }
    } 

}