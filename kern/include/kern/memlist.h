
#ifndef _MEMLIST_H_
#define _MEMLIST_H_

#include <spinlock.h>

struct memlist_node {
	paddr_t paddr;
	size_t sz;
    bool allocated;
    int otherpages;
	struct memlist_node *next;
    struct memlist_node *prev;    
};

struct memlist {
    paddr_t start; 
    paddr_t end;
	struct memlist_node *head;
    struct spinlock ml_lk;
};

/* Function definitions */
struct memlist* memlist_create(paddr_t start, paddr_t end); 
struct memlist* memlist_bootstrap(paddr_t start, paddr_t end);
struct memlist_node *memlist_node_create(struct memlist_node *prev, struct memlist_node *next);
void memlist_destroy(struct memlist *ml);
paddr_t memlist_get_first_fit(struct memlist *ml, size_t sz);
void memlist_remove(struct memlist *ml, paddr_t blk);
struct memlist* memlist_copy(struct memlist *src, struct memlist *dst);
struct memlist_node* memlist_get_node(struct memlist *ml, vaddr_t addr);
void memlist_node_set_otherpages(struct memlist_node *n, int otherpages);
int memlist_node_get_otherpages(struct memlist_node *n);


#endif /* _VM_H_ */
