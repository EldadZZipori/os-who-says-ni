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
    if (fl->flk == NULL)
    {
        kfree(fl->head);
        kfree(fl);
        return NULL;
    }

    fl->start start; 
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
vaddr_t freelist_get_first_fit(freelist *fl, size_t sz) {
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
        if (cur->sz == sz)
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
        elif (cur->sz > sz) 
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
    struct freelist_node cur = fl->head; 
    struct freelist_node new; 

    while (cur->next != NULL) // stop at last node for special case that blk exists after last free block
    { 
        if (cur->addr > blk) // blk is in previous allocated block
        // so clear prev block 
        {
            new = kmalloc(sizeof(struct freelist_node));
            new->addr = blk;
            new->sz = sz;
            new->next = cur;
            new->prev = cur->prev;
            cur->prev->next = new;
            cur->prev = new;
        }
    }

    // special case: blk is after last free block
    if (cur->addr < blk) 
    {
        new = kmalloc(sizeof(struct freelist_node));
        new->addr = blk;
        new->sz = sz;
        new->next = NULL;
        new->prev = cur;
        cur->next = new;
    }

    // merge blocks if possible

    // merge new node with previous node if possible
    if (new->prev != NULL && new->prev->addr + new->prev->sz == new->addr) 
    {
        new->prev->sz += new->sz;
        new->prev->next = new->next;
        new->next->prev = new->prev;
        kfree(new);
    }

    // merge new node with next node if possible
    if (new->next != NULL && new->addr + new->sz == new->next->addr) 
    {
        new->sz += new->next->sz;
        new->next = new->next->next;
        new->next->prev = new;
        kfree(new->next);
    }

    return;
}