#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <kern/freelist.h>


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
    fl->head->sz = end - start;
    fl->head->allocated = false;
    fl->head->next = NULL;
    fl->head->prev = NULL;

    return fl;
}

struct freelist_node *freelist_node_create(struct freelist_node *prev, struct freelist_node *next)
{
    struct freelist_node *new = kmalloc(sizeof(struct freelist_node));
    
    if (new == NULL) return NULL;
     
    new->next = next; 
    new->prev = prev; 
    new->otherpages = -1;
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
 * @param sz size of the block to allocate
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
void* freelist_get_first_fit(struct freelist *fl, size_t sz) {
    KASSERT(fl != NULL);
    KASSERT(sz > 0);
    struct freelist_node *cur = fl->head;
    while (cur != NULL) 
    {
        // remove the block from the freelist
        if (cur->sz == sz) // perfect fit, just set to allocated and dont change ptrs
        { 
            cur->allocated = true;
            return cur->addr; 
        }
        else if (cur->sz > sz) 
        {
            // split block into 
            // [ allocd, sz = sz] [ not_allocd, sz = prevsz - sz]
            struct freelist_node *new = kmalloc(sizeof(struct freelist_node));
            new->addr = cur->addr + sz; // start after allocd block
            new->sz = cur->sz - sz; // current sz - alloc sz
            new->allocated = false; 
            new->prev = cur;
            new->next = cur->next;
            // fix up allocd block 
            cur->sz = sz; 
            cur->allocated = true; 
            cur->next = new; 
            return cur->addr;
        }
    } 
    return NULL;
}

/* Free a block */
void freelist_remove(struct freelist *fl, void *blk, size_t sz)
{
    KASSERT(fl != NULL);
    KASSERT(fl->head != NULL);
    KASSERT(blk < fl->end);
    KASSERT(blk >= fl->start);

    struct freelist_node *cur = fl->head; 
    struct freelist_node *new = kmalloc(sizeof(struct freelist_node)); 

    new->addr = blk; 
    new->sz = sz; 

    // Case 1: if freelist is empty, add node
    if (cur == NULL) {
        fl->head = new;
        new->next = NULL;
        new->prev = NULL;
        return;
    }

    // find the allocated block we want to free
    while (cur != NULL) 
    { 
        if (cur->addr != blk) // not this one
        {
            cur = cur->next;
        }
    }

    // either cur = allocd block we want to free
    // or cur is at end of list and now NULL
    if (cur != NULL)
    { 
        // make sure we allocated this
        KASSERT(cur->allocated);

        // free and fix up pointers
        // case 1: prev and next blocks are allocated or NULL
        // no merge
        if ((cur->prev == NULL || cur->prev->allocated) && (cur->next == NULL || cur->next->allocated))
        {
            cur->allocated = false; // freed up now
        }

        // case 2: prev not allocated, next allocated or NULL
        // delete cur and merge sizes into prev 
        else if ((cur->prev != NULL && !cur->prev->allocated))
        {
            cur->prev->sz += cur->sz; // merge sizes
            cur->prev->next = cur->next; // fix forward ptr
            cur->next->prev = cur->prev; // fix backward ptr
            kfree(cur);
        }

        // case 3: prev allocated or NULL, next not allocated
        // delete next and merge sizes into cur
        else if (cur->next != NULL && !cur->next->allocated) 
        { 
            cur->sz += cur->next->sz; // merge sizes
            cur->next = cur->next->next; // fix forward ptr
            cur->next->prev = cur; // fix backward ptr
            cur->allocated = false; // set to be a large free blk 
            kfree(cur->next);
        }

        // case 4: prev and next both unallocated 
        // delete cur and next and merge sizes into prev
        else if (cur->prev != NULL && !cur->prev->allocated && cur->next != NULL && !cur->next->allocated)
        {
            cur->prev->sz += (cur->sz + cur->next->sz); // merge sizes
            cur->prev->next = cur->next->next; // fix forward ptr
            cur->next->prev = cur->prev; // fix backward ptr 
            kfree(cur->next);
            kfree(cur);
        }
    }
    else
    {
        panic("freelist_remove: tried to free a pointer we never allocated\n");
    }
}

void freelist_node_set_otherpages(struct freelist_node *n, int otherpages)
{
    if (n == NULL) return; 
    n->otherpages = otherpages;
    return;
}