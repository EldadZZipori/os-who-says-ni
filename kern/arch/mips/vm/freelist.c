#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <freelist.h>


/* Freelist-related functions */

struct freelist* freelist_create(vaddr_t start, vaddr_t end) {

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

    fl->addr = start;
    fl->size = end - start;
    fl->head->next = NULL;
    fl->head->prev = NULL;

    return fl;
}

void freelist_destroy(struct freelist *fl) {
    struct freelist_node *cur = fl->head;
    struct freelist_node *next;

    while (cur != NULL) 
    {
        next = cur->next;
        kfree(cur);
        cur = next;
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
vaddr_t freelist_get_first_fit(freelist *fl, size_t size) {
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
        if (cur->size == size)
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
        elif (cur->size > size) 
        {
            // allocate at start of block
            cur->addr += size;
            cur->size -= size;
            return cur->addr - size;
        }
    } 
    return NULL;
}

