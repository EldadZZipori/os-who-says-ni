#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <kern/memlist.h>


/**
 * @brief create a memlist for an empty virtual or physical memory region
 * 
 * @param start void pointer, representing a physical or virtual address,
 *              of the start of the memory region (inclusive)
 * @param end void pointer, representing a physical or virtual address, 
 *              of the end of hte memory region (exclusive)
*/
struct memlist* memlist_create(paddr_t start, paddr_t end) {

    struct memlist *ml = kmalloc(sizeof(struct memlist));
    if (ml == NULL) 
    {
        return NULL;
    }

    ml->head = kmalloc(sizeof(struct memlist_node));
    if (ml->head == NULL) 
    {
        kfree(ml);
        return NULL;
    }

    ml->ml_lk = lock_create("memlist lock");
    if (ml->ml_lk == NULL)
    {
        kfree(ml->head);
        kfree(ml);
        return NULL;
    }

    ml->start = start; 
    ml->end = end;
    ml->head->paddr = start;
    ml->head->sz = end - start;
    ml->head->allocated = false;
    ml->head->next = NULL;
    ml->head->prev = NULL;

    return ml;
}

struct memlist_node *memlist_node_create(struct memlist_node *prev, struct memlist_node *next)
{
    struct memlist_node *new = kmalloc(sizeof(struct memlist_node));
    
    if (new == NULL) return NULL;
     
    new->next = next; 
    new->prev = prev; 
    new->otherpages = -1;

    return new;
}

void memlist_destroy(struct memlist *ml) {
    KASSERT(ml != NULL);

    struct memlist_node *cur = ml->head->next;

    while (cur != NULL) 
    {
        kfree(cur->prev);
        cur = cur->next;
    }
    
    lock_destroy(ml->ml_lk);
    kfree(ml);
}

/** 
 * @brief gets a free block and updates the memlist accordingly 
 * 
 * @param ml memlist 
 * @param sz size of the block to allocate
 * Case 1: Find block equal to size 
 * e.g.: get_first_fit(100) 
 * 
 * Before: 
 * ml -> 40 -> 60 -> 100 -> 200 -> NULL
 * 
 * After: 
 *             100B allocated 
 *                 v 
 * ml -> 40 -> 60 -> 200 -> NULL
 * 
 * 
 * 
 * Case 2: Find block larger than size
 * e.g. get_first_fit(70) 
 * 
 * Before: 
 * ml -> 40 -> 60 -> 100 -> 200 -> NULL
 * 
 * After: 
 *              70B allocated
 *                  v                   
 * ml -> 40 -> 60 -> 30 -> 200 -> NULL
 */
paddr_t memlist_get_first_fit(struct memlist *ml, size_t sz) {
    KASSERT(ml != NULL);
    KASSERT(sz > 0);
    struct memlist_node *cur = ml->head;
    while (cur != NULL) 
    {
        // remove the block from the memlist
        if (cur->sz == sz && cur->allocated == false) // perfect fit, just set to allocated and dont change ptrs
        { 
            cur->allocated = true;
            return cur->paddr; 
        }
        else if (cur->sz > sz && cur->allocated == false) 
        {
            // split block into 
            // [ allocd, sz = sz] [ not_allocd, sz = prevsz - sz]
            struct memlist_node *new = kmalloc(sizeof(struct memlist_node));
            new->paddr = cur->paddr + sz; // start after allocd block
            new->sz = cur->sz - sz; // current sz - alloc sz
            new->allocated = false; 
            new->prev = cur;
            new->next = cur->next;
            // fix up allocd block 
            cur->sz = sz; 
            cur->allocated = true; 
            cur->next = new; 
            return cur->paddr;
        }
        cur = cur->next;
    } 
    return (paddr_t)NULL;
}

/* Free a block */
void memlist_remove(struct memlist *ml, paddr_t blk)
{
    KASSERT(ml != NULL);
    KASSERT(ml->head != NULL);
    KASSERT(blk < ml->end);
    KASSERT(blk >= ml->start);

    struct memlist_node *cur = ml->head; 

    // find the allocated block we want to free
    while (cur != NULL) 
    { 
        if (cur->paddr != blk) // not this one
        {
            cur = cur->next;
        }
        else
        {
            break;
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
            if (cur->next != NULL) cur->next->prev = cur->prev; // fix backward ptr
            kfree(cur);
        }

        // case 3: prev allocated or NULL, next not allocated
        // delete next and merge sizes into cur
        else if (cur->next != NULL && !cur->next->allocated) 
        { 
            cur->sz += cur->next->sz; // merge sizes
            cur->next = cur->next->next; // fix forward ptr
            if (cur->next != NULL) cur->next->prev = cur; // fix backward ptr
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
        panic("memlist_remove: tried to free a pointer we never allocated\n");
    }
}

struct memlist *memlist_copy(struct memlist *src, struct memlist *dst)
{
    KASSERT(src != NULL);
    KASSERT(dst != NULL);

    if (src->head == NULL) return NULL; // nothing to copy

    if (dst->head == NULL) {
        dst->head = kmalloc(sizeof(struct memlist_node));
        if (dst->head == NULL) return NULL;
    }

    struct memlist_node *cur_src = src->head;
    struct memlist_node *cur_dst = dst->head;


    while (cur_src->next != NULL) 
    {
        if (cur_dst->next == NULL) 
        {
            cur_dst->next = kmalloc(sizeof(struct memlist_node));
            if (cur_dst->next == NULL) return NULL;

            cur_dst->next->prev = cur_dst; // set prev ptr
        }
        // copy fields 
        cur_dst->paddr = cur_src->paddr;
        cur_dst->sz = cur_src->sz;
        cur_dst->allocated = cur_src->allocated;

        cur_src = cur_src->next;
        cur_dst = cur_dst->next;
    }

    return dst;
}

struct memlist_node* memlist_get_node(struct memlist *ml, paddr_t paddr) 
{
    // find the node with the address
    struct memlist_node *cur = ml->head;
    while (cur != NULL) 
    {
        if (cur->paddr == paddr) 
        {
            return cur;
        }
        cur = cur->next;
    }

    return NULL; // not in memlist

}

void memlist_node_set_otherpages(struct memlist_node *n, int otherpages)
{
    if (n == NULL) return; 
    n->otherpages = otherpages;
    return;
}

int memlist_node_get_otherpages(struct memlist_node *n) 
{
    if (n == NULL) return -1; 
    return n->otherpages;
}